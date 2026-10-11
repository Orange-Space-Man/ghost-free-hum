#include "game/models/s3dmorph.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        data.at(offset) = std::uint8_t(value >> 8);
        data.at(offset + 1) = std::uint8_t(value);
    }

    void dword(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        word(data, offset, value >> 16);
        word(data, offset + 2, value);
    }

    void number(std::vector<std::uint8_t>& data, unsigned offset, float value) {
        unsigned bits;
        std::memcpy(&bits, &value, 4);
        dword(data, offset, bits);
    }

    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

int main() {
    try {
        std::vector<std::uint8_t> shapes(160), keys(160);
        word(shapes, 8, 1);
        dword(shapes, 16, 48);
        dword(shapes, 20, 68);
        dword(shapes, 24, 84);
        dword(shapes, 32, 108);
        dword(shapes, 40, 112);
        dword(shapes, 44, 116);
        word(shapes, 64, 2);
        word(shapes, 66, 1);
        word(shapes, 78, 1);
        number(shapes, 84, 2);
        number(shapes, 88, 4);
        number(shapes, 92, 6);
        number(shapes, 96, 10);
        number(shapes, 100, 12);
        number(shapes, 104, 14);
        word(shapes, 108, 0xe000);
        word(shapes, 110, 1);
        word(shapes, 114, 1);
        word(shapes, 116, 1);
        word(keys, 4, 60);
        dword(keys, 16, 48);
        dword(keys, 20, 120);
        dword(keys, 24, 128);
        dword(keys, 28, 132);
        dword(keys, 124, 8);
        word(keys, 128, 0x8002);
        word(keys, 130, 2);
        const float weights[]{0,1,0,2, 60,1,0,0, 0,3,0, 60,3,0};
        for (unsigned i = 0; i < 14; ++i) number(keys, 48 + i * 4, weights[i]);
        s3dmorph::Animation animation;
        std::string error;
        check(animation.read(shapes, keys, 2, error), error.c_str());
        std::vector<std::array<float, 3>> positions{{99,98,97},{0,0,0}};
        animation.apply(0, positions);
        check(positions[0][0] == 99 && positions[1] == std::array<float,3>{7,8,9}, "Signed targets, normalization, or unaffected vertices.");
        animation.apply(30, positions);
        check(std::abs(positions[1][0] - 27.5f / 4.25f) < 0.00001f, "Split tangents or 60 Hz Hermite time scale.");
        animation.apply(30.5f, positions);
        check(std::isfinite(positions[1][0]) && positions[1][0] != 27.5f / 4.25f, "Fractional morph sampling.");
        animation.apply(60, positions);
        check(positions[1] == std::array<float,3>{7,8,9}, "Morph endpoint clamping.");
        check(animation.advance(59.75f, 0.5f) == 0.25f, "Morph loop wrap.");
        auto invalid = shapes;
        word(invalid, 116, 2);
        check(!animation.read(invalid, keys, 2, error), "Accepted invalid destination.");
        animation.apply(0, positions);
        check(positions[1][0] == 7, "Failed read damaged previous animation.");
        word(shapes, 64, 1);
        word(keys, 6, 1);
        check(animation.read(shapes, keys, 2, error), error.c_str());
        animation.apply(30, positions);
        check(positions[1] == std::array<float,3>{-2,-4,-6}, "Single-target morph must have unit weight.");
        check(animation.advance(59.75f, 0.5f) == 60, "Morph hold endpoint.");
        std::cout << "Morph checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
