#include "game/models/s3dshadow.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void put(std::vector<std::uint8_t>& data, unsigned offset, unsigned value, unsigned bytes = 2) {
        for (unsigned i = 0; i < bytes; ++i) data.at(offset + i) = std::uint8_t(value >> ((bytes - i - 1) * 8));
    }

    void number(std::vector<std::uint8_t>& data, unsigned offset, float value) {
        unsigned bits;
        std::memcpy(&bits, &value, 4);
        put(data, offset, bits, 4);
    }

    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

int main() {
    try {
        std::vector<std::uint8_t> scene(256), settings(0x25c);
        put(scene, 4, 1);
        put(scene, 16, 32, 4);
        put(scene, 24, 160, 4);
        put(scene, 28, 188, 4);
        put(settings, 0x22e, 1);
        for (unsigned i = 0; i < 7; ++i) {
            put(scene, 32 + 0x14 + i * 6 + 2, i);
            put(scene, 32 + 0x14 + i * 6 + 4, 1);
            number(scene, 160 + i * 4, float(i + 1));
        }
        for (unsigned i = 0; i < 3; ++i) {
            put(scene, 32 + 0x50 + i * 6 + 2, i);
            put(scene, 32 + 0x50 + i * 6 + 4, 1);
        }
        put(scene, 188, 10);
        put(scene, 190, unsigned(-20));
        put(scene, 192, 300);
        for (unsigned i = 0; i < 6; ++i) number(settings, 0x230 + i * 4, float((i + 1) * 10));
        number(settings, 0x248, 26);
        number(settings, 0x24c, 16);
        number(settings, 0x250, 8192);
        for (unsigned i = 0; i < 4; ++i) put(settings, 0x254 + i * 2, i == 3 ? 128 : 100);
        s3dshadow::Animation shadow;
        std::string error;
        check(shadow.read(scene, settings, error), error.c_str());
        auto projection = shadow.sample(0.5f);
        check(projection.position == std::array<float,3>{11,22,33} && projection.target == std::array<float,3>{44,55,66}, "Shadow position/target curves.");
        check(projection.fieldOfView == 33 && projection.nearPlane == 16 && projection.farPlane == 8192, "Shadow projection fields.");
        check(projection.color == std::array<float,4>{110.0f/255,80.0f/255,1,128.0f/255}, "Shadow color curves and fixed alpha.");
        check(!shadow.read(scene, {0}, error), "Truncated shadow settings accepted.");
        check(shadow.sample(0).fieldOfView == 33, "Failed read changed shadow state.");
        put(settings, 0x22c, 0xffff);
        number(settings, 0x23c, 10);
        number(settings, 0x244, 30);
        number(settings, 0x248, 200);
        check(shadow.read(scene, settings, error), error.c_str());
        projection = shadow.sample(0);
        check(projection.target[2] == 40 && projection.fieldOfView == 179, "Shadow vertical-view fallback or FOV clamp.");
        put(settings, 0x22e, 0);
        check(shadow.read(scene, settings, error), error.c_str());
        projection = shadow.sample(0);
        check(projection.position == std::array<float,3>{0,200,1000} && projection.target == std::array<float,3>{0,200,-1000}
            && projection.fieldOfView == 50 && projection.nearPlane == 1 && projection.farPlane == 1, "Original disabled-shadow defaults.");
        std::cout << "Shadow projection checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
