#pragma once

#include "game/world/dooranimationpose.h"
#include "game/math/originalfloattrig.h"

namespace modelmaterial {
    struct TextureTransform {
        std::uint32_t flags = 0;
        std::array<float, 7> values{};
    };

    bool readTransform(const std::vector<std::uint8_t>& bytes, int index, TextureTransform& transform, std::string& error);
    dooranimation::Matrix evaluateTransform(const TextureTransform& transform, const dooranimation::Matrix& basis,
        const originalmath::FloatTrig& trig);
}
