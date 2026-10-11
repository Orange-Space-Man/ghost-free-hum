#include "game/world/roommaterials.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace {
    using Color = std::array<std::uint8_t, 4>;

    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Texture data is truncated.");
    }

    unsigned read16(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    std::size_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (static_cast<std::size_t>(read16(data, offset)) << 16) | read16(data, offset + 2);
    }

    Color rgb565(unsigned value) {
        const unsigned r = value >> 11;
        const unsigned g = (value >> 5) & 63;
        const unsigned b = value & 31;
        return {static_cast<std::uint8_t>((r << 3) | (r >> 2)),
            static_cast<std::uint8_t>((g << 2) | (g >> 4)),
            static_cast<std::uint8_t>((b << 3) | (b >> 2)), 255};
    }
}

bool roommaterials::decodeTexture(const std::vector<std::uint8_t>& data, unsigned width, unsigned height,
    unsigned format, Texture& texture, std::string& error) {
    texture = {};
    try {
        require(width > 0 && height > 0 && width <= 1024 && height <= 1024, "Invalid texture dimensions.");
        if (format > 6 && format != 14) {
            throw std::runtime_error("Unsupported room texture format " + std::to_string(format));
        }
        const unsigned tile = format <= 2 || format == 14 ? 8 : 4;
        const unsigned tileHeight = format == 1 || format == 2 ? 4 : tile;
        const unsigned tileBytes = format == 6 ? 64 : 32;
        range(data, 0, static_cast<std::size_t>((width + tile - 1) / tile) * ((height + tileHeight - 1) / tileHeight) * tileBytes);
        Texture result{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
        const auto pixel = [&](unsigned x, unsigned y, const Color& color) {
            if (x < width && y < height) {
                std::copy(color.begin(), color.end(), result.pixels.begin() + (static_cast<std::size_t>(y) * width + x) * 4);
            }
        };
        std::size_t offset = 0;
        for (unsigned y = 0; y < height; y += tileHeight) {
            for (unsigned x = 0; x < width; x += tile) {
                if (format != 14) {
                    for (unsigned row = 0; row < tileHeight; ++row) {
                        for (unsigned column = 0; column < tile; ++column) {
                            const unsigned index = row * tile + column;
                            Color color{};
                            if (format == 0) {
                                const auto value = static_cast<std::uint8_t>(((data[offset + index / 2] >> (index % 2 == 0 ? 4 : 0)) & 15) * 17);
                                color = {value, value, value, value};
                            } else if (format == 1) {
                                const auto intensity = data[offset + index];
                                color = {intensity, intensity, intensity, intensity};
                            } else if (format == 2) {
                                const auto intensity = static_cast<std::uint8_t>((data[offset + index] & 15) * 17);
                                const auto alpha = static_cast<std::uint8_t>((data[offset + index] >> 4) * 17);
                                color = {intensity, intensity, intensity, alpha};
                            } else if (format == 3) {
                                const auto intensity = data[offset + index * 2 + 1];
                                color = {intensity, intensity, intensity, data[offset + index * 2]};
                            } else if (format == 6) {
                                color = {data[offset + index * 2 + 1], data[offset + 32 + index * 2],
                                    data[offset + 33 + index * 2], data[offset + index * 2]};
                            } else {
                                const auto value = read16(data, offset + index * 2);
                                if (format == 4) {
                                    color = rgb565(value);
                                } else if ((value & 0x8000) != 0) {
                                    for (unsigned channel = 0; channel < 3; ++channel) {
                                        const unsigned component = (value >> (10 - channel * 5)) & 31;
                                        color[channel] = static_cast<std::uint8_t>((component << 3) | (component >> 2));
                                    }
                                    color[3] = 255;
                                } else {
                                    for (unsigned channel = 0; channel < 3; ++channel) {
                                        color[channel] = static_cast<std::uint8_t>(((value >> (8 - channel * 4)) & 15) * 17);
                                    }
                                    const unsigned alpha = (value >> 12) & 7;
                                    color[3] = static_cast<std::uint8_t>((alpha << 5) | (alpha << 2) | (alpha >> 1));
                                }
                            }
                            pixel(x + column, y + row, color);
                        }
                    }
                    offset += tileBytes;
                    continue;
                }
                for (unsigned block = 0; block < 4; ++block) {
                    const unsigned first = read16(data, offset);
                    const unsigned second = read16(data, offset + 2);
                    std::array<Color, 4> colors{rgb565(first), rgb565(second), Color{}, Color{}};
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        colors[2][channel] = static_cast<std::uint8_t>(first > second ?
                            (5 * colors[0][channel] + 3 * colors[1][channel]) / 8 : (colors[0][channel] + colors[1][channel]) / 2);
                        colors[3][channel] = static_cast<std::uint8_t>(first > second ?
                            (3 * colors[0][channel] + 5 * colors[1][channel]) / 8 : colors[2][channel]);
                    }
                    colors[2][3] = 255;
                    colors[3][3] = first > second ? 255 : 0;
                    for (unsigned row = 0; row < 4; ++row) {
                        for (unsigned column = 0; column < 4; ++column) {
                            const unsigned index = (data[offset + 4 + row] >> (6 - column * 2)) & 3;
                            pixel(x + (block % 2) * 4 + column, y + (block / 2) * 4 + row, colors[index]);
                        }
                    }
                    offset += 8;
                }
            }
        }
        texture = std::move(result);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool roommaterials::read(const std::vector<std::uint8_t>& data, const roomgeometry::Mesh& mesh,
    Collection& collection, std::string& error) {
    collection = {};
    try {
        const auto textures = read32(data, 0x0c);
        const auto samplers = read32(data, 0x10);
        const auto positions = read32(data, 0x14);
        const auto materials = read32(data, 0x34);
        const auto batches = read32(data, 0x38);
        require(positions >= 0x60 && positions <= materials && materials < batches && batches <= data.size(), "Invalid material sections.");
        Collection result;
        result.textures.push_back({1, 1, {255, 255, 255, 255}});
        std::map<unsigned, std::size_t> loaded;
        std::size_t decodedBytes = 0;
        for (const auto& triangle : mesh.triangles) {
            const auto index = triangle.material;
            require(index < (batches - materials) / 40, "Invalid material index.");
            if (result.materials.size() <= index) result.materials.resize(index + 1);
            const auto sampler = read16(data, materials + index * 40 + 8);
            if (sampler == 0xffff) continue;
            require(textures >= 0x60 && textures < samplers && samplers < positions, "Missing texture or sampler section.");
            require(sampler < (positions - samplers) / 20, "Invalid sampler index.");
            const auto header = samplers + sampler * 20;
            const auto textureIndex = read16(data, header);
            require(data[header + 4] <= 2 && data[header + 5] <= 2, "Invalid texture wrapping.");
            if (!loaded.count(textureIndex)) {
                const auto record = textures + textureIndex * 12;
                require(record <= samplers && samplers - record >= 12, "Invalid texture index.");
                const auto start = textures + read32(data, record + 8);
                const auto width = read16(data, record);
                const auto height = read16(data, record + 2);
                const unsigned format = data[record + 4];
                const unsigned tile = format <= 2 || format == 14 ? 8 : 4;
                const unsigned tileHeight = format == 1 || format == 2 ? 4 : tile;
                require(width <= 1024 && height <= 1024, "Invalid texture dimensions.");
                const std::size_t size = static_cast<std::size_t>((width + tile - 1) / tile) * ((height + tileHeight - 1) / tileHeight) * (format == 6 ? 64 : 32);
                require(start <= samplers && size <= samplers - start, "Texture exceeds its section.");
                require(result.textures.size() < 1024, "Too many room textures.");
                decodedBytes += static_cast<std::size_t>(width) * height * 4;
                require(decodedBytes <= 256 * 1024 * 1024, "Room exceeds the texture memory limit.");
                Texture texture;
                const std::vector<std::uint8_t> bytes(data.begin() + start, data.begin() + start + size);
                if (!decodeTexture(bytes, width, height, format, texture, error)) throw std::runtime_error(error);
                loaded[textureIndex] = result.textures.size();
                result.textures.push_back(std::move(texture));
            }
            result.materials[index] = {loaded.at(textureIndex), data[header + 4], data[header + 5]};
        }
        collection = std::move(result);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
