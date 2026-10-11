#pragma once

#include "game/player/playercontrollerstate.h"

namespace playerinput {
    struct PadStatus {
        std::uint16_t buttons = 0;
        std::int8_t stickX = 0, stickY = 0, substickX = 0, substickY = 0;
        std::uint8_t triggerLeft = 0, triggerRight = 0, analogA = 0, analogB = 0;
        std::int8_t error = 0;
    };

    class PadSampler {
    public:
        explicit PadSampler(const std::vector<std::uint8_t>& executable);
        void update(Pad& pad, PadStatus status) const;

    private:
        std::array<std::uint8_t, 8> clamp;
        std::array<float, 4> thresholds;
        bool normalize;
        unsigned stick(std::int8_t x, std::int8_t y, float divisor, std::array<float, 2>& result, float& magnitude) const;
    };
}
