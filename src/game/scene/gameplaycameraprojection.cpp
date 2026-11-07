#include "game/scene/gameplaycameraprojection.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr unsigned shakeDecayAddress = 0x804d9c88;
    constexpr unsigned halfAngleAddress = 0x804dd708, radiansAddress = 0x804dd70c;

    float constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    void normalize(std::array<float, 3>& value) {
        const float inverse = 1.0f / std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
        for (auto& component : value) component *= inverse;
    }
}

gameplaycamera::Projection::Projection(const std::vector<std::uint8_t>& executable) :
    shakeDecay(constant(executable, shakeDecayAddress)), halfAngle(constant(executable, halfAngleAddress)),
    radians(constant(executable, radiansAddress)) {}

gameplaycamera::DrawState gameplaycamera::Projection::publish(CameraOwner& owner, const actormodel::InitialPose& pose) const {
    owner.fieldA4 = static_cast<std::uint16_t>(owner.fieldA4 + 0x2000);
    owner.fieldA8 -= shakeDecay;
    if (owner.fieldA8 < 0) {
        owner.fieldA8 = 0;
        owner.fieldA4 = 0;
    }
    std::array<float, 3> direction;
    for (unsigned i = 0; i < 3; ++i) direction[i] = owner.target[i] - owner.position[i];
    if (owner.fieldA4 != 0) {
        const float length = std::sqrt((direction[2] * direction[2] + direction[0] * direction[0]) + direction[1] * direction[1]);
        const auto heading = pose.angle(direction[2], direction[0]);
        const auto pitch = static_cast<std::int16_t>(pose.angle(
            std::sqrt(direction[0] * direction[0] + direction[2] * direction[2]), direction[1]));
        const auto tilted = static_cast<std::uint16_t>(static_cast<int>(float(pitch) +
            owner.fieldA8 * pose.horizontalDirection(owner.fieldA4)[0]));
        const auto vertical = pose.horizontalDirection(tilted), horizontal = pose.horizontalDirection(heading);
        const float radius = length * vertical[2];
        direction = {radius * horizontal[0], length * vertical[0], radius * horizontal[2]};
    }
    DrawState result{};
    result.visibilityMode = owner.visibilityMode;
    result.camera.position = owner.position;
    result.camera.up = owner.up;
    result.camera.fieldOfView = owner.fieldOfView;
    result.camera.aspect = owner.aspect;
    result.camera.nearPlane = owner.nearPlane;
    result.camera.farPlane = owner.farPlane;
    std::array<float, 3> backward;
    for (unsigned i = 0; i < 3; ++i) {
        result.camera.target[i] = owner.position[i] + direction[i];
        backward[i] = owner.position[i] - result.camera.target[i];
    }
    normalize(backward);
    auto right = cross(owner.up, backward);
    normalize(right);
    const std::array<std::array<float, 3>, 3> rows{right, cross(backward, right), backward};
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 3; ++column) result.view[row * 4 + column] = rows[row][column];
        result.view[row * 4 + 3] = -((owner.position[2] * rows[row][2] + owner.position[0] * rows[row][0]) +
            owner.position[1] * rows[row][1]);
    }
    auto& projection = result.projection;
    const float inverseDepth = float(1.0 / double(owner.farPlane - owner.nearPlane));
    if (owner.flags & 2) {
        const auto& bounds = owner.orthographic;
        const float inverseWidth = 1.0f / (bounds[3] - bounds[2]);
        const float inverseHeight = 1.0f / (bounds[0] - bounds[1]);
        const float twiceNear = 2.0f * owner.nearPlane;
        projection[0] = twiceNear * inverseWidth;
        projection[2] = inverseWidth * (bounds[3] + bounds[2]);
        projection[5] = twiceNear * inverseHeight;
        projection[6] = inverseHeight * (bounds[0] + bounds[1]);
    } else {
        const float angle = radians * float(double(halfAngle) * owner.fieldOfView);
        const float cotangent = float(1.0 / double(std::tan(angle)));
        projection[0] = float(double(cotangent) / owner.aspect);
        projection[5] = cotangent;
    }
    projection[10] = float(-double(owner.nearPlane) * inverseDepth);
    projection[11] = float(double(inverseDepth) * -double(owner.farPlane * owner.nearPlane));
    projection[14] = -1;
    for (unsigned row = 0; row < 4; ++row) for (unsigned column = 0; column < 4; ++column) {
        owner.matrix[row * 4 + column] = ((projection[row * 4 + 3] * (column == 3 ? 1.0f : 0.0f) +
            projection[row * 4 + 2] * result.view[8 + column]) + projection[row * 4] * result.view[column]) +
            projection[row * 4 + 1] * result.view[4 + column];
    }
    result.worldProjection = owner.matrix;
    return result;
}
