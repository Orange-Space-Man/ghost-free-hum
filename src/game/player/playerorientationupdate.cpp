#include "game/player/playerorientationupdate.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    using playerorientation::Vector;
    using playerorientation::Quaternion;

    float dot(const Vector& first, const Vector& second) {
        return (first[0] * second[0] + first[1] * second[1]) + first[2] * second[2];
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    float normalize(Vector& value) {
        const float magnitude = std::sqrt(dot(value, value));
        if (magnitude >= 0.0001f) {
            const float factor = 1.0f / magnitude;
            for (auto& component : value) component *= factor;
        } else value.fill(0);
        return magnitude;
    }

    void normalize(Quaternion& value) {
        const float squared = ((value[3] * value[3] + value[2] * value[2]) + value[0] * value[0]) + value[1] * value[1];
        if (squared == 0) return;
        const float factor = 1.0f / std::sqrt(squared);
        for (auto& component : value) component *= factor;
    }

    Vector column(const actormodel::Matrix& matrix, unsigned index) {
        Vector result{matrix[index], matrix[index + 4], matrix[index + 8]};
        normalize(result);
        return result;
    }

    std::uint16_t angle(float degrees) {
        return static_cast<std::uint16_t>(static_cast<std::int32_t>((65536.0f * degrees) / 360.0f));
    }
}

playerorientation::Quaternion playerorientation::heading(const Vector& value, bool inverted) {
    Vector forward = value, up{0, inverted ? -1.0f : 1.0f, 0};
    normalize(forward);
    Vector difference;
    for (unsigned i = 0; i < 3; ++i) difference[i] = up[i] - forward[i];
    if (std::sqrt(dot(difference, difference)) <= 0.0001f) up = {0, 0, 1};
    auto right = cross(up, forward);
    normalize(right);
    up = cross(forward, right);
    normalize(up);
    return fromMatrix({right[0], up[0], forward[0], 0, right[1], up[1], forward[1], 0,
        right[2], up[2], forward[2], 0});
}

playerorientation::Quaternion playerorientation::multiply(const Quaternion& a, const Quaternion& b) {
    return {(a[1] * b[2] + a[0] * b[3] + a[3] * b[0]) - a[2] * b[1],
        (a[2] * b[0] + a[1] * b[3] + a[3] * b[1]) - a[0] * b[2],
        (a[0] * b[1] + a[2] * b[3] + a[3] * b[2]) - a[1] * b[0],
        ((a[3] * b[3] - a[0] * b[0]) - a[1] * b[1]) - a[2] * b[2]};
}

playerorientation::Quaternion playerorientation::interpolate(double weight, const Quaternion& a, const Quaternion& b) {
    const double inverse = static_cast<float>(1.0 - weight);
    const float product = ((a[3] * b[3] + a[2] * b[2]) + a[0] * b[0]) + a[1] * b[1];
    if (product < 0) weight = -weight;
    Quaternion result;
    for (unsigned i = 0; i < 4; ++i) result[i] = static_cast<float>(inverse * a[i] + weight * b[i]);
    return result;
}

float playerorientation::update(playercomponents::QuaternionState& state, Quaternion& output, const Quaternion& source,
    const Input& input, const actormodel::InitialPose& pose) {
    const auto original = source;
    if (!state.field10) throw std::runtime_error("Player orientation requires the original retained angular-velocity value.");
    auto& angularVelocity = *state.field10;
    bool changed = false;
    unsigned mode = input.mode;
    angularVelocity *= input.damping;
    if (std::abs(angularVelocity) < 1) angularVelocity = 0;
    if (mode == 3 && input.enabled && input.turning && input.turning->value >= input.threshold) {
        angularVelocity += input.turning->second[0] * -input.acceleration * input.turning->value;
        angularVelocity = std::clamp(angularVelocity, -input.maximumSpeed, input.maximumSpeed);
    }
    if ((mode == 1 || mode == 2) && angularVelocity != 0) mode = 3;
    const auto retained = toMatrix(state.quaternion);
    const auto up = column(retained, 1);
    if ((!input.inverted && up[1] < 0) || (input.inverted && up[1] > 0))
        state.quaternion = heading(column(retained, 2), input.inverted);
    const bool moving = input.enabled && input.movement && input.movement->value >= input.threshold;
    switch (mode) {
    case 0:
        if (moving) {
            Vector forward{input.velocity[0], 0, input.velocity[2]};
            if (normalize(forward) > 0) {
                state.quaternion = heading(forward, input.inverted);
                changed = true;
            }
        }
        break;
    case 1:
        if (moving) {
            state.quaternion = heading(input.movement->second, input.inverted);
            changed = true;
        }
        break;
    case 2:
        if (moving) {
            const float limit = input.turnLimit * input.movement->value;
            const float cosine = pose.horizontalDirection(angle(limit))[2];
            const float projection = dot(direction(original), input.movement->second);
            const auto target = heading(input.movement->second, input.inverted);
            if (cosine <= projection) state.quaternion = target;
            else {
                float sine = 1.0f - projection * projection;
                if (sine > 0) sine = std::sqrt(sine);
                float difference = (360.0f / 65536.0f) * pose.angle(projection, sine);
                if (difference >= 180) difference -= 360;
                state.quaternion = interpolate(limit / difference, original, target);
            }
        }
        break;
    case 3:
        if (angularVelocity != 0) {
            const auto turn = pose.horizontalDirection(angle(0.5f * (input.inverted ? -angularVelocity : angularVelocity)));
            state.quaternion = multiply(original, {0, turn[0], 0, turn[2]});
            normalize(state.quaternion);
        }
        break;
    case 4:
        state.quaternion = input.target;
        break;
    case 5:
    case 6: {
        Vector forward;
        for (unsigned i = 0; i < 3; ++i) forward[i] = mode == 5 ? input.targetPosition[i] - input.position[i] :
            input.position[i] - input.targetPosition[i];
        const float magnitude = normalize(forward);
        if (mode == 6 || magnitude > 1) state.quaternion = heading(forward, input.inverted);
        break;
    }
    case 7: {
        auto forward = input.targetDirection;
        normalize(forward);
        state.quaternion = heading(forward, input.inverted);
        break;
    }
    }
    if (input.mode != 3) angularVelocity = 0;
    if (changed && input.reverse) {
        auto matrix = toMatrix(state.quaternion);
        for (unsigned i : {0u, 2u, 4u, 6u, 8u, 10u}) matrix[i] = -matrix[i];
        state.quaternion = fromMatrix(matrix);
    }
    output = interpolate(std::min(input.blend, 1.0f), original, state.quaternion);
    normalize(output);
    const auto target = toMatrix(state.quaternion), current = toMatrix(output);
    return (dot(column(target, 0), column(current, 0)) + dot(column(target, 1), column(current, 1))) +
        dot(column(target, 2), column(current, 2));
}
