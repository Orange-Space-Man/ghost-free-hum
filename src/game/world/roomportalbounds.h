#pragma once

#include "game/world/roomtraversalstate.h"

namespace roomportal {
    using Point = std::array<float, 3>;
    using Matrix = std::array<float, 16>;

    std::array<Point, 4> corners(const std::array<std::int32_t, 3>& center,
        std::int16_t width, std::int16_t height);
    std::array<float, 4> transform(const Matrix& matrix, const Point& point);
    roomtraversal::Bounds project(const std::array<Point, 4>& points, const Matrix& matrix,
        const roomtraversal::Bounds& viewport, const roomtraversal::Bounds& parent);
    roomtraversal::Bounds clip(std::uint8_t positiveDepthCount, const roomtraversal::Bounds& parent,
        roomtraversal::Bounds bounds);
    roomtraversal::Bounds enclose(const std::array<std::int16_t, 4>& x,
        const std::array<std::int16_t, 4>& y);
}
