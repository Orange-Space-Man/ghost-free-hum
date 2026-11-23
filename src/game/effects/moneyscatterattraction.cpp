#include "moneyscatterattraction.h"
#include "game/player/playervacuummodel.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    using Vector = moneyscatter::Vector;

    float magnitude(const Vector& value) {
        return std::sqrt(std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1]);
    }

    Vector normalized(const Vector& value) {
        const float length = magnitude(value);
        if (length <= 0) return value;
        return {value[0] / length, value[1] / length, value[2] / length};
    }
}

int moneyscatter::attract(const playeractor::View& player, const particles::SuctionContext& context,
    float radius, Particle& particle) {
    if (player.field8A0 != 1 || !player.field8B4) {
        particle.acceleration = {};
        return 0;
    }
    if (!context.position || !context.direction)
        throw std::logic_error("Coin attraction requires the refreshed player suction context.");
    const auto& origin = *context.position;
    const Vector delta{particle.position[0] - origin[0], particle.position[1] - origin[1],
        particle.position[2] - origin[2]};
    const float distance = magnitude(delta);
    if (distance <= 25.0f) return 2;

    Vector direction;
    if (!player.field8B4->intersects(radius, direction, particle.position)) {
        particle.acceleration = {};
        return 0;
    }

    constexpr float reach = 150.0f, transverseRadius = 25.0f, transverseScale = 60.0f;
    constexpr float forceScale = 150000.0f, maximumVelocity = 15.0f;
    const float inverseDistance = reach / distance;
    const float force = std::max((reach / (distance * distance)) * forceScale, 0.0f);
    const auto& axis = *context.direction;
    const float projection = std::fma(delta[2], axis[2], std::fma(delta[0], axis[0], delta[1] * axis[1]));
    const Vector transverse{axis[0] * projection - delta[0], axis[1] * projection - delta[1],
        axis[2] * projection - delta[2]};
    const float transverseDistance = magnitude(transverse);
    const Vector correction = normalized(transverse);
    const float correctionSpeed = std::min(inverseDistance * context.strength, transverseDistance);
    const float fraction = std::min(transverseDistance / transverseRadius, 1.0f);
    const float damping = std::max(1.0f - inverseDistance * (transverseScale * fraction), 0.0f);
    for (unsigned i = 0; i < 3; ++i)
        particle.acceleration[i] = (particle.acceleration[i] + correction[i] * correctionSpeed) * damping;
    for (unsigned i = 0; i < 3; ++i) particle.velocity[i] = direction[i] * force;
    const float speed = magnitude(particle.velocity);
    if (maximumVelocity < speed)
        for (auto& component : particle.velocity) component *= maximumVelocity / speed;
    return 1;
}
