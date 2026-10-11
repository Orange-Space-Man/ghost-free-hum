#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/menu/gamefont.h"

#include <algorithm>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 2, "Truncated font resource.");
        return (data[offset] << 8) | data[offset + 1];
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }
}

bool gamefont::Font::readExecutable(const std::vector<std::uint8_t>& dol, std::string& error) {
    std::vector<std::uint8_t> header, data, ascii;
    if (!dolresources::read(dol, glme01::fontResource, 16, header, error) ||
        !dolresources::read(dol, glme01::fontResource, dword(header, 8), data, error) ||
        !dolresources::read(dol, glme01::fontAsciiTable, 190, ascii, error)) return false;
    return read(data, ascii, error);
}

bool gamefont::Font::read(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& ascii, std::string& error) {
    error.clear();
    try {
        require(data.size() >= 32 && dword(data, 0) == 0x464f4e54 && dword(data, 4) == 0x62666e31 &&
            dword(data, 8) == data.size(), "Invalid BFN font header.");
        Font result;
        bool information = false;
        unsigned encoding = 0, minimumCode = 0xffff;
        std::map<unsigned, std::array<unsigned, 2>> widths;
        std::size_t offset = 32;
        for (unsigned block = 0; block < dword(data, 12); ++block) {
            const auto kind = dword(data, offset), size = dword(data, offset + 4);
            require(size >= 8 && size <= data.size() - offset, "Invalid font block length.");
            const std::vector<std::uint8_t> bytes(data.begin() + offset, data.begin() + offset + size);
            if (kind == 0x494e4631) {
                require(!information, "Duplicate font information.");
                encoding = word(bytes, 8);
                result.ascent = word(bytes, 10);
                result.descent = word(bytes, 12);
                result.width = word(bytes, 14);
                result.leading = word(bytes, 16);
                result.fallback = word(bytes, 18);
                information = true;
            } else if (kind == 0x4d415031) {
                const auto mode = word(bytes, 8), first = word(bytes, 10), last = word(bytes, 12);
                require(first <= last, "Invalid font map range.");
                minimumCode = std::min(minimumCode, first);
                if (mode == 3) {
                    for (unsigned index = 0; index < word(bytes, 14); ++index) {
                        result.mapping.emplace(word(bytes, 16 + index * 4), word(bytes, 18 + index * 4));
                    }
                } else {
                    require(mode == 0 || mode == 2, "Unsupported font mapping mode.");
                    for (unsigned code = first; code <= last; ++code) {
                        result.mapping.emplace(code, mode == 0 ? code - first : word(bytes, 16 + (code - first) * 2));
                    }
                }
            } else if (kind == 0x57494431) {
                const auto first = word(bytes, 8), last = word(bytes, 10);
                require(first <= last, "Invalid font width range.");
                for (unsigned code = first; code <= last; ++code) {
                    const auto value = word(bytes, 12 + (code - first) * 2);
                    widths.emplace(code, std::array<unsigned, 2>{value >> 8, value & 255});
                }
            } else if (kind == 0x474c5931) {
                const auto first = word(bytes, 8), last = word(bytes, 10);
                const auto cellWidth = word(bytes, 12), cellHeight = word(bytes, 14), sheetSize = dword(bytes, 16);
                const auto format = word(bytes, 20), columns = word(bytes, 22), rows = word(bytes, 24);
                const auto sheetWidth = word(bytes, 26), sheetHeight = word(bytes, 28);
                require(first <= last && cellWidth && cellHeight && columns && rows && sheetSize &&
                    sheetWidth <= 1024 && sheetHeight <= 1024 &&
                    columns * cellWidth <= sheetWidth && rows * cellHeight <= sheetHeight && format == 0,
                    "Unsupported font glyph sheet.");
                require(sheetWidth % 8 == 0 && cellHeight % 8 == 0 && sheetHeight % 8 == 0 &&
                    sheetSize == sheetWidth * sheetHeight / 2, "Invalid font sheet stride.");
                const auto capacity = columns * rows, count = last - first + 1;
                const auto base = static_cast<unsigned>(result.textures.size());
                for (unsigned page = 0; page * capacity < count; ++page) {
                    const auto used = std::min(capacity, count - page * capacity);
                    const auto usedRows = (used + columns - 1) / columns;
                    const auto required = usedRows * cellHeight * sheetWidth / 2;
                    const std::size_t start = 32 + static_cast<std::size_t>(page) * sheetSize;
                    require(start <= bytes.size() && required <= bytes.size() - start, "Truncated font glyph sheet.");
                    std::vector<std::uint8_t> sheet(sheetSize, 0);
                    std::copy_n(bytes.begin() + start, std::min<std::size_t>(sheetSize, bytes.size() - start), sheet.begin());
                    roommaterials::Texture texture;
                    if (!roommaterials::decodeTexture(sheet, sheetWidth, sheetHeight, format, texture, error)) throw std::runtime_error(error);
                    for (std::size_t pixel = 0; pixel < texture.pixels.size(); pixel += 4) texture.pixels[pixel + 3] = texture.pixels[pixel];
                    result.textures.push_back(std::move(texture));
                }
                for (unsigned index = 0; index < count; ++index) {
                    result.glyphs.emplace(first + index, Glyph{base + index / capacity,
                        index % columns * cellWidth, index % capacity / columns * cellHeight});
                }
            } else {
                throw std::runtime_error("Unsupported font block.");
            }
            offset += size;
        }
        require(information && result.width && result.ascent + result.descent && result.glyphs.count(result.fallback), "Incomplete font.");
        for (auto& entry : result.glyphs) {
            const auto found = widths.find(entry.first);
            entry.second.left = found == widths.end() ? 0 : found->second[0];
            entry.second.advance = found == widths.end() ? result.width : found->second[1];
        }
        if (encoding == 2 && minimumCode > 7999) {
            require(ascii.size() == 190, "Missing original ASCII font mapping.");
            for (unsigned character = 32; character < 127; ++character) {
                const auto found = result.mapping.find(word(ascii, (character - 32) * 2));
                result.mapping[character] = found == result.mapping.end() ? result.fallback : found->second;
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const gamefont::Glyph& gamefont::Font::glyph(unsigned character) const {
    const auto found = mapping.find(character);
    return glyphs.at(found == mapping.end() ? fallback : found->second);
}

float gamefont::Font::draw(unsigned character, float x, float baseline, float requestedWidth, float height,
    bool continuation, startupscene::Picture& picture) const {
    const auto& entry = glyph(character);
    const auto& texture = textures.at(entry.texture);
    const float scale = requestedWidth / width;
    picture = {};
    picture.texture = entry.texture;
    picture.x = x - (continuation ? entry.left * scale : 0);
    picture.y = baseline - ascent * height / (ascent + descent);
    picture.width = requestedWidth;
    picture.height = height;
    const float u0 = static_cast<float>((entry.x * 32768) / texture.width) / 32768;
    const float v0 = static_cast<float>((entry.y * 32768) / texture.height) / 32768;
    const float u1 = static_cast<float>(((entry.x + width) * 32768) / texture.width) / 32768;
    const float v1 = static_cast<float>(((entry.y + ascent + descent) * 32768) / texture.height) / 32768;
    picture.coordinates = {{{u0,v0}, {u1,v0}, {u0,v1}, {u1,v1}}};
    return (entry.advance + (continuation ? 0 : entry.left)) * scale;
}
