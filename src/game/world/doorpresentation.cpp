#include "game/world/doorpresentation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

unsigned doors::distanceOpacity(const std::array<float, 3>& position, const std::array<float, 3>& player) {
    const float x = position[0] - player[0], y = position[1] - player[1], z = position[2] - player[2];
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = xx + yy;
    const float squared = zz + xy;
    const float distance = std::sqrt(squared);
    if (distance <= 300) return 100;
    const float excess = distance - 300;
    const float reduction = 0.2f * excess;
    if (!std::isfinite(reduction) || reduction >= 100) return 0;
    return 100 - static_cast<unsigned>(reduction);
}

std::uint8_t doors::advanceOpacity(std::uint8_t current, bool distanceControlled, unsigned target) {
    if (!distanceControlled) return current < 251 ? static_cast<std::uint8_t>(current + 5) : 255;
    if (target > 100) throw std::out_of_range("Door distance opacity target.");
    const int value = current, destination = static_cast<int>(target);
    return static_cast<std::uint8_t>(destination < value ? std::max(value - 5, destination) : std::min(value + 5, destination));
}
