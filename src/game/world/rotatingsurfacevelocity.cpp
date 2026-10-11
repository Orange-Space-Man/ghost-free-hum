#include "game/world/rotatingsurfacevelocity.h"

#include <cmath>

bool actors::RotatingSurfaces::velocity(const actormodel::InitialPose& pose, const std::array<float, 3>& position,
    std::array<float, 3>& result, playerorientation::Quaternion& orientation) const {
    for (const auto& entry : entries) {
        if (entry.radius <= 0) continue;
        const std::array<float, 3> delta{entry.position[0] - position[0], 0, entry.position[2] - position[2]};
        const float square = std::fma(delta[2], delta[2], delta[0] * delta[0]) + delta[1] * delta[1];
        const float length = std::sqrt(square);
        if (length >= entry.radius || !entry.rotation) continue;
        const float degrees = (360.0f / 65536.0f) * entry.rotation;
        const float half = 0.5f * degrees;
        const float angle = (65536.0f * half) / 360.0f;
        const auto direction = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<std::int32_t>(angle)));
        orientation = {0, direction[0], 0, direction[2]};
        const auto matrix = playerorientation::toMatrix(orientation);
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = row * 4;
            const float rotated = std::fma(matrix[offset + 2], delta[2], matrix[offset] * delta[0]) +
                (matrix[offset + 3] + matrix[offset + 1] * delta[1]);
            result[row] = rotated - delta[row];
        }
        return true;
    }
    return false;
}
