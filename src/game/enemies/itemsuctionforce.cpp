#include "game/enemies/itemsuctionforce.h"
#include "game/player/playervacuummodel.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    using Vector = itemsuction::Vector;

    float magnitude(const Vector& value) {
        return std::sqrt(std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1]);
    }

    Vector normalized(const Vector& value) {
        const float square = std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1];
        const float estimate = 1.0f / std::sqrt(square);
        const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
        return {value[0] * inverse, value[1] * inverse, value[2] * inverse};
    }

    std::int32_t integer(float value) {
        if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0) {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }
}

itemsuction::Force::Force(const playeractor::View& player, const particles::SuctionContext& context) :
    player(player), context(context) {}

unsigned itemsuction::Force::apply(double arrivalRadius, const Vector& position, Vector& velocity, Vector& acceleration,
    std::uint8_t& remaining, std::uint8_t& duration, bool countdown, const Parameters& parameters,
    const Vector& unwrittenDirection) {
    const auto miss = [&] {
        acceleration = {};
        if (countdown) duration = 0;
        return 0u;
    };
    if (player.field8A0 != 1 || !player.field8B4) return miss();
    if (!context.position || !context.direction) throw std::runtime_error("Item suction requires the original scene context.");
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - (*context.position)[i];
    distance = magnitude(delta);
    Vector outward = unwrittenDirection;
    if (!(distance <= 0)) outward = normalized(delta);
    if (distance <= arrivalRadius && duration) {
        if (countdown) duration = 0;
        return 2;
    }
    const auto& axis = *context.direction;
    const float projection = std::fma(outward[0], axis[0], outward[1] * axis[1]) + outward[2] * axis[2];
    if (projection <= -0.35f || !player.field8B4->intersects(200, direction, position)) return miss();
    accelerate(position, velocity, acceleration);
    if (countdown) {
        if (!duration) {
            duration = static_cast<std::uint8_t>(integer(static_cast<float>(parameters.durationBase) + parameters.durationScale / distance));
            remaining = duration;
        }
        else if (remaining) {
            --remaining;
            const float amount = -(1.0f + (parameters.braking * static_cast<float>(remaining)) /
                static_cast<float>(duration)) * magnitude(velocity);
            for (unsigned i = 0; i < 3; ++i) velocity[i] += direction[i] * amount;
        }
    }
    return 1;
}

void itemsuction::Force::accelerate(const Vector& position, Vector& velocity, Vector& acceleration) {
    const float inverseDistance = context.strength / distance;
    const float inverseSquare = context.strength / (distance * distance);
    const float rawSpeed = inverseSquare * 150000.0f;
    const float speed = rawSpeed >= 5.5f ? rawSpeed : 5.5f;
    const float pull = inverseDistance * 800.0f;
    Vector delta, transverse;
    const auto& axis = *context.direction;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - (*context.position)[i];
    const float projection = std::fma(delta[0], axis[0], delta[1] * axis[1]) + delta[2] * axis[2];
    for (unsigned i = 0; i < 3; ++i) transverse[i] = axis[i] * projection - delta[i];
    const float transverseDistance = magnitude(transverse);
    const Vector transverseDirection = transverseDistance <= 0 ? transverse : normalized(transverse);
    const float amount = pull >= transverseDistance ? transverseDistance : pull;
    const float ratio = transverseDistance / 25.0f;
    const float boundedRatio = ratio <= 1 ? ratio : 1;
    const float rawDrag = 1.0f - inverseDistance * (60.0f * boundedRatio);
    const float drag = rawDrag >= 0 ? rawDrag : 0;
    for (unsigned i = 0; i < 3; ++i) {
        acceleration[i] = (transverseDirection[i] * amount + acceleration[i]) * drag;
        velocity[i] = direction[i] * speed;
    }
    if (15 < magnitude(velocity)) {
        velocity = normalized(velocity);
        for (auto& component : velocity) component *= 15;
    }
}
