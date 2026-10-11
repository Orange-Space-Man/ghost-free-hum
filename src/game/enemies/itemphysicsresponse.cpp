#include "game/enemies/itemphysicsresponse.h"

std::array<float, 3> itemphysics::reflection(const mapcollision::State& collision, std::int16_t polygon,
    const std::array<float, 3>& velocity, float reflect, float friction) {
    std::array<float, 3> normal;
    if (!collision.polygonNormal(polygon, normal)) return {};
    const float x = velocity[0] * normal[0];
    const float y = velocity[1] * normal[1];
    const float z = velocity[2] * normal[2];
    const float dot = z + (x + y);
    if (dot >= 0) return {};
    const float bounce = -(1.0f + reflect);
    const float drag = -friction;
    std::array<float, 3> result;
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float projected = normal[axis] * dot;
        const float tangent = velocity[axis] - projected;
        result[axis] = projected * bounce + tangent * drag;
    }
    return result;
}
