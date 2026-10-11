#pragma once

#include "game/player/playerpadsampler.h"

namespace playerinput {
    struct HostPad {
        std::uint16_t buttons = 0;
        std::int16_t stickX = 0, stickY = 0, substickX = 0, substickY = 0;
        std::uint8_t triggerLeft = 0, triggerRight = 0;
    };

    struct HostSample {
        PadStatus status;
        std::int16_t port = -1;
    };

    PadStatus translate(const HostPad& pad);
    HostSample pollWindowPad(void* window, bool enabled);
}
