#pragma once

#include <array>
#include <cstdint>

namespace doors {
    unsigned distanceOpacity(const std::array<float, 3>& position, const std::array<float, 3>& player);
    std::uint8_t advanceOpacity(std::uint8_t current, bool distanceControlled, unsigned target);
}
