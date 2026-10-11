#include "game/menu/gamefont.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    void put(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> ((bytes - i - 1) * 8));
    }

    std::size_t block(std::vector<std::uint8_t>& data, unsigned kind, unsigned size) {
        const auto offset = data.size();
        data.resize(offset + size);
        put(data, offset, kind, 4);
        put(data, offset + 4, size, 4);
        return offset;
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(32);
        put(data, 0, 0x464f4e54, 4);
        put(data, 4, 0x62666e31, 4);
        put(data, 12, 4, 4);
        auto offset = block(data, 0x494e4631, 32);
        put(data, offset + 10, 6, 2);
        put(data, offset + 12, 2, 2);
        put(data, offset + 14, 8, 2);
        put(data, offset + 16, 9, 2);
        offset = block(data, 0x4d415031, 16);
        put(data, offset + 10, 32, 2);
        put(data, offset + 12, 33, 2);
        offset = block(data, 0x474c5931, 96);
        put(data, offset + 10, 1, 2);
        put(data, offset + 12, 8, 2);
        put(data, offset + 14, 8, 2);
        put(data, offset + 16, 32, 4);
        put(data, offset + 22, 1, 2);
        put(data, offset + 24, 1, 2);
        put(data, offset + 26, 8, 2);
        put(data, offset + 28, 8, 2);
        data[offset + 32] = 0xf0;
        data[offset + 64] = 0x70;
        offset = block(data, 0x57494431, 16);
        put(data, offset + 10, 1, 2);
        put(data, offset + 12, 0x0105, 2);
        put(data, offset + 14, 0x0204, 2);
        put(data, 8, static_cast<unsigned>(data.size()), 4);
        return data;
    }
}

int main() {
    try {
        gamefont::Font font;
        std::string error;
        auto data = fixture();
        require(font.read(data, {}, error), error);
        require(font.textures.size() == 2 && font.textures[1].pixels[3] == 119, "Font sheet decoding.");
        require(font.glyph(33).texture == 1 && font.glyph(33).advance == 4 && font.glyph(999).texture == 0, "Font mapping and fallback.");
        startupscene::Picture picture;
        require(font.draw(32, 10, 20, 16, 16, true, picture) == 10 && picture.x == 8 && picture.y == 8 &&
            picture.height == 16 && picture.coordinates[3][0] == 1, "Font baseline, bearing and scale.");
        require(font.draw(32, 10, 20, 16, 16, false, picture) == 12 && picture.x == 10, "First character bearing.");
        for (std::size_t size = 0; size < data.size(); ++size) {
            std::vector<std::uint8_t> truncated(data.begin(), data.begin() + size);
            if (size >= 12) put(truncated, 8, static_cast<unsigned>(size), 4);
            require(!font.read(truncated, {}, error), "Accepted truncated font.");
        }
        require(font.textures.size() == 2, "Failed read changed the loaded font.");
        put(data, 84, 0xfffffff0, 4);
        require(!font.read(data, {}, error), "Accepted oversized font block.");
        std::cout << "Font checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
