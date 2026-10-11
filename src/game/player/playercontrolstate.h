#pragma once

#include <array>
#include <cstdint>

namespace playercontrols {
    struct Settings {
        std::array<std::uint32_t, 54> fields;
        std::uint16_t flags;
        void configure(int mode);
    };

    struct Motion {
        float value = 0;
        std::array<float, 3> first{}, second{};
    };

    class State {
    public:
        explicit State(const Settings* settings = nullptr);
        void reset();
        void configure(const Settings* value);
        void setMode(int value);
        std::array<std::uint8_t, 4> flags0;
        float field4, field8;
        std::array<std::uint8_t, 18> flagsC;
        float field20;
        std::array<std::uint8_t, 5> flags24;
        float field2C;
        std::array<std::uint8_t, 7> flags30;
        std::array<Motion, 5> motions;
        std::uint8_t fieldC4, fieldC5;
        float fieldC8;
        std::uint8_t fieldCC;
        Settings settings;
        int mode = 0;
        std::array<std::uint8_t, 16> fields1B0;
        std::array<Motion, 4> history;
    };
}
