#pragma once

#include <array>
#include <cstdint>

namespace controllerfeedback {
    struct PulseSlot {
        bool active = false;
        float amplitude = 0;
        std::uint32_t remaining = 0;
        std::uint32_t fadeRemaining = 0;
        std::uint32_t fade = 0;
        float inverseFade = 0;
    };

    class Pulses {
    public:
        std::array<PulseSlot, 4> slots;

        void play(double amplitude, std::uint32_t duration, std::uint32_t fade);
        int update(std::uint32_t frame);
        void reset();
    };
}
