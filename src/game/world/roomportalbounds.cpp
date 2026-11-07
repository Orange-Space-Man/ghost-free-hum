#include "game/world/roomportalbounds.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
    float coordinate(std::int32_t center, int offset) {
        const std::uint32_t bits = static_cast<std::uint32_t>(center) + static_cast<std::uint32_t>(offset);
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return static_cast<float>(value);
    }

    std::int16_t narrow(int value) {
        const unsigned low = static_cast<unsigned>(value) & 0xffff;
        return static_cast<std::int16_t>(low < 0x8000 ? static_cast<int>(low) : static_cast<int>(low) - 0x10000);
    }

    std::int16_t screenCoordinate(float normalized, int origin, int extent) {
        const float scaled = normalized * static_cast<float>(extent);
        const float half = scaled * 0.5f;
        const float offset = static_cast<float>(extent) * 0.5f;
        const float centered = half + offset;
        const float screen = static_cast<float>(origin) + centered;
        if (!std::isfinite(screen) || static_cast<double>(screen) < -2147483648.0 ||
            static_cast<double>(screen) >= 2147483648.0)
            throw std::runtime_error("Portal screen coordinate exceeds supported integer conversion.");
        return narrow(static_cast<std::int32_t>(screen));
    }
}

std::array<roomportal::Point, 4> roomportal::corners(const std::array<std::int32_t, 3>& center,
    std::int16_t width, std::int16_t height) {
    const float left = coordinate(center[0], -(width / 2));
    const float right = coordinate(center[0], width / 2);
    const float bottom = coordinate(center[1], -(height / 2));
    const float top = coordinate(center[1], height / 2);
    const float z = static_cast<float>(center[2]);
    return {{{left, bottom, z}, {right, bottom, z}, {right, top, z}, {left, top, z}}};
}

std::array<float, 4> roomportal::transform(const Matrix& matrix, const Point& point) {
    std::array<float, 4> result;
    for (unsigned row = 0; row < 4; ++row) {
        const float x = matrix[row * 4] * point[0];
        const float y = matrix[row * 4 + 1] * point[1];
        const float z = matrix[row * 4 + 2] * point[2];
        const float xy = x + y;
        const float xyz = z + xy;
        result[row] = matrix[row * 4 + 3] + xyz;
    }
    return result;
}

roomtraversal::Bounds roomportal::project(const std::array<Point, 4>& points, const Matrix& matrix,
    const roomtraversal::Bounds& viewport, const roomtraversal::Bounds& parent) {
    std::array<std::int16_t, 4> x, y;
    std::uint8_t positiveDepthCount = 0;
    for (unsigned i = 0; i < points.size(); ++i) {
        const auto projected = transform(matrix, points[i]);
        if (projected[3] == 0) throw std::runtime_error("Portal projection has undefined zero-W coordinates.");
        const float reciprocal = 1.0f / projected[3];
        const float depth = projected[2] * reciprocal;
        const float horizontal = projected[0] * reciprocal;
        const float vertical = projected[1] * reciprocal;
        if (depth > 0) ++positiveDepthCount;
        x[i] = screenCoordinate(horizontal, viewport[0], int(viewport[2]) - viewport[0]);
        y[i] = screenCoordinate(-1.0f * vertical, viewport[1], int(viewport[3]) - viewport[1]);
    }
    return clip(positiveDepthCount, parent, enclose(x, y));
}

roomtraversal::Bounds roomportal::clip(std::uint8_t positiveDepthCount,
    const roomtraversal::Bounds& parent, roomtraversal::Bounds bounds) {
    if (positiveDepthCount == 4 || bounds[0] == bounds[2] || bounds[1] == bounds[3]) return {};
    if (parent[2] < bounds[0] || parent[3] < bounds[1] || parent[0] > bounds[2] || parent[1] > bounds[3]) return {};
    if (bounds[0] < parent[0]) bounds[0] = parent[0];
    if (bounds[1] < parent[1]) bounds[1] = parent[1];
    if (parent[2] < bounds[2]) bounds[2] = parent[2];
    if (parent[3] < bounds[3]) bounds[3] = parent[3];
    if (bounds[0] == bounds[2] || bounds[1] == bounds[3]) return {};
    return bounds;
}

roomtraversal::Bounds roomportal::projectVisible(const std::array<Point, 4>& points, const Matrix& matrix,
    const roomtraversal::Bounds& viewport, const roomtraversal::Bounds& parent) {
    std::array<std::array<float, 4>, 4> projected;
    unsigned inside = 0;
    // GX clip depth spans [-w, 0]; clip before dividing or narrowing screen coordinates.
    for (unsigned i = 0; i < points.size(); ++i) {
        projected[i] = transform(matrix, points[i]);
        if (projected[i][2] + projected[i][3] >= 0) ++inside;
    }
    if (inside == 4) return project(points, matrix, viewport, parent);
    if (!inside) return {};
    std::vector<std::array<float, 4>> clipped;
    auto previous = projected.back();
    float previousDistance = previous[2] + previous[3];
    for (const auto& current : projected) {
        const float distance = current[2] + current[3];
        if ((distance >= 0) != (previousDistance >= 0)) {
            const float fraction = previousDistance / (previousDistance - distance);
            std::array<float, 4> intersection;
            for (unsigned i = 0; i < 4; ++i) intersection[i] = previous[i] + fraction * (current[i] - previous[i]);
            clipped.push_back(intersection);
        }
        if (distance >= 0) clipped.push_back(current);
        previous = current;
        previousDistance = distance;
    }
    std::array<float, 4> bounds{float(parent[2]), float(parent[3]), float(parent[0]), float(parent[1])};
    for (const auto& point : clipped) {
        const float x = viewport[0] + (point[0] / point[3] + 1) * (viewport[2] - viewport[0]) * 0.5f;
        const float y = viewport[1] + (1 - point[1] / point[3]) * (viewport[3] - viewport[1]) * 0.5f;
        bounds[0] = std::min(bounds[0], x - 1);
        bounds[1] = std::min(bounds[1], y - 1);
        bounds[2] = std::max(bounds[2], x + 1);
        bounds[3] = std::max(bounds[3], y + 1);
    }
    return clip(0, parent, {static_cast<std::int16_t>(std::clamp(bounds[0], float(parent[0]), float(parent[2]))),
        static_cast<std::int16_t>(std::clamp(bounds[1], float(parent[1]), float(parent[3]))),
        static_cast<std::int16_t>(std::clamp(bounds[2], float(parent[0]), float(parent[2]))),
        static_cast<std::int16_t>(std::clamp(bounds[3], float(parent[1]), float(parent[3])))});
}

roomtraversal::Bounds roomportal::enclose(const std::array<std::int16_t, 4>& x,
    const std::array<std::int16_t, 4>& y) {
    return {narrow(*std::min_element(x.begin(), x.end()) - 1), narrow(*std::min_element(y.begin(), y.end()) - 1),
        narrow(*std::max_element(x.begin(), x.end()) + 1), narrow(*std::max_element(y.begin(), y.end()) + 1)};
}
