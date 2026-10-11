#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/startup/startupresources.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace {
    void require(bool condition, const char* error) {
        if (!condition) throw std::runtime_error(error);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Truncated startup resource.");
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    std::uint32_t dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    std::string tag(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 4);
        return std::string(data.begin() + offset, data.begin() + offset + 4);
    }
}


bool startupresources::decodeTexture(const std::vector<std::uint8_t>& data, roommaterials::Texture& texture, std::string& error) {
    texture = {};
    error.clear();
    try {
        range(data, 0, 32);
        require(data[8] == 0, "Paletted startup textures are not implemented.");
        const auto offset = dword(data, 28);
        require(offset >= 32 && offset <= data.size(), "Invalid startup texture offset.");
        std::vector<std::uint8_t> pixels(data.begin() + offset, data.end());
        if (!roommaterials::decodeTexture(pixels, word(data, 2), word(data, 4), data[0], texture, error)) return false;
        if (data[0] == 0) {
            for (std::size_t i = 0; i < texture.pixels.size(); i += 4) texture.pixels[i + 3] = texture.pixels[i];
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool startupresources::readLayout(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& archive,
    Resources& resources, std::string& error) {
    error.clear();
    try {
        require(tag(data, 0) == "SCRN" && tag(data, 4) == "blo1" && dword(data, 8) == data.size(), "Unsupported title layout.");
        auto result = resources;
        result.panes.clear();
        std::size_t offset = 32;
        int depth = 0;
        bool ended = false;
        while (offset < data.size()) {
            const auto kind = tag(data, offset);
            const auto size = dword(data, offset + 4);
            require(size >= 8, "Invalid title layout block.");
            range(data, offset, size);
            if (kind == "INF1") {
                require(size == 16 && word(data, offset + 8) == 640 && word(data, offset + 10) == 480, "Unsupported title canvas.");
            } else if (kind == "PAN1") {
                require(size == 24 && tag(data, offset + 12) == "ROOT" && word(data, offset + 16) == 0 &&
                    word(data, offset + 18) == 0, "Unsupported title pane hierarchy.");
            } else if (kind == "BGN1") {
                require(++depth == 1, "Nested title panes are not implemented.");
            } else if (kind == "END1") {
                require(depth-- == 1, "Invalid title pane hierarchy.");
            } else if (kind == "EXT1") {
                require(depth == 0 && offset + size == data.size(), "Invalid title layout terminator.");
                ended = true;
            } else if (kind == "PIC1") {
                require(depth == 1 && size >= 24, "Invalid title picture.");
                Pane pane;
                const auto fields = data[offset + 8];
                require(fields >= 6 && fields <= 9, "Unsupported title pane fields.");
                pane.visible = data[offset + 9] != 0;
                pane.tag = tag(data, offset + 12);
                pane.x = static_cast<std::int16_t>(word(data, offset + 16));
                pane.y = static_cast<std::int16_t>(word(data, offset + 18));
                pane.width = static_cast<std::int16_t>(word(data, offset + 20));
                pane.height = static_cast<std::int16_t>(word(data, offset + 22));
                require(pane.width > 0 && pane.height > 0, "Invalid title picture bounds.");
                std::size_t cursor = offset + 24;
                if (fields >= 7) {
                    require(word(data, cursor) == 0, "Rotated title panes are not implemented.");
                    cursor += 2;
                }
                if (fields >= 8) require(data.at(cursor++) == 0, "Unsupported title pane anchor.");
                if (fields >= 9) pane.alpha = data.at(cursor++);
                cursor = (cursor + 3) & ~std::size_t(3);
                require(cursor + 3 <= offset + size && data[cursor++] == 3 && data[cursor++] == 2, "Unsupported title texture reference.");
                const auto length = data[cursor++];
                require(length > 0 && cursor + length + 3 <= offset + size, "Truncated title texture reference.");
                std::string name(data.begin() + cursor, data.begin() + cursor + length);
                for (auto& character : name) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
                cursor += length;
                require(data[cursor] == 0 && data[cursor + 1] == 0 && data[cursor + 2] == 15, "Unsupported title picture binding.");
                std::vector<std::uint8_t> bti;
                roommaterials::Texture texture;
                if (!roomarchive::read(archive, "timg/" + name, bti, error) || !decodeTexture(bti, texture, error)) throw std::runtime_error(error);
                pane.texture = result.textures.size();
                result.textures.push_back(std::move(texture));
                result.panes.push_back(pane);
            } else {
                throw std::runtime_error("Unsupported title layout block: " + kind);
            }
            offset += size;
        }
        require(ended && !result.panes.empty(), "Incomplete title layout.");
        resources = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool startupresources::read(const std::filesystem::path& path, Resources& resources, std::string& error) {
    resources = {};
    error.clear();
    try {
        gameassets::Package package;
        std::vector<std::uint8_t> dol, header, archive, compressed, decoded;
        if (!package.open(path, error) || !package.read("sys/main.dol", dol, error) ||
            !dolresources::read(dol, glme01::titleArchive, 8, header, error)) throw std::runtime_error(error);
        require(tag(header, 0) == "RARC", "Unsupported boot archive.");
        if (!dolresources::read(dol, glme01::titleArchive, dword(header, 4), archive, error) ||
            !roomarchive::read(archive, "nintendo.szp", compressed, error) ||
            !roomarchive::decompress(compressed, decoded, error)) throw std::runtime_error(error);
        Resources result;
        roommaterials::Texture logo;
        if (!decodeTexture(decoded, logo, error)) throw std::runtime_error(error);
        result.textures.push_back(std::move(logo));
        if (!roomarchive::read(archive, "ninlogo.pcm", decoded, error)) throw std::runtime_error(error);
        require(decoded.size() == 57 * 560 * 4, "Unexpected boot PCM length.");
        for (std::size_t i = 0; i < decoded.size(); i += 2) result.logoSamples.push_back(static_cast<std::int16_t>(word(decoded, i)));
        if (!roomarchive::read(archive, "res_titl.szp", compressed, error) ||
            !roomarchive::decompress(compressed, decoded, error)) throw std::runtime_error(error);
        std::vector<std::uint8_t> layout;
        if (!roomarchive::read(decoded, "scrn/testtitl.blo", layout, error) || !readLayout(layout, decoded, result, error)) throw std::runtime_error(error);
        if (!dolresources::read(dol, glme01::sineTable, 4096 * 4, decoded, error)) throw std::runtime_error(error);
        for (std::size_t i = 0; i < decoded.size(); i += 4) {
            const auto bits = dword(decoded, i);
            float value;
            std::memcpy(&value, &bits, 4);
            require(std::isfinite(value), "Invalid original title sine table.");
            result.sine.push_back(value);
        }
        if (!dolresources::read(dol, glme01::initialSaveFlags, 31, result.initialSaveFlags, error) ||
            !jaudio::read(package, result.audio, error)) throw std::runtime_error(error);
        resources = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
