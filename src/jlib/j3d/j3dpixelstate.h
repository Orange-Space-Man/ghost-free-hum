#pragma once

#include "jlib/j3d/j3dmaterialfactory.h"
#include <optional>

namespace j3dmodel {
    struct PixelState {
        std::array<std::uint8_t, 5> alpha{7, 0, 0, 7, 0};
        std::array<std::uint8_t, 4> blend{0, 1, 0, 3};
        std::array<std::uint8_t, 3> depth{1, 3, 1};
        std::uint8_t depthLocation = 1, dither = 0;
        std::optional<std::array<std::uint8_t, 44>> fog;
    };

    void loadPixelState(const MaterialPixelBlock& block, PixelState& state);
}
