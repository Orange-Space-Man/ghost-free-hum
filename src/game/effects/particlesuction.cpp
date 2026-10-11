#include "game/effects/particlesuction.h"
#include "game/player/playervacuummodel.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    using Vector = std::array<float, 3>;

    float magnitude(const Vector& vector) {
        return std::sqrt(std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1]);
    }

    Vector normalized(const Vector& vector) {
        const float square = std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1];
        const float estimate = 1.0f / std::sqrt(square);
        const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
        return {vector[0] * inverse, vector[1] * inverse, vector[2] * inverse};
    }
}

particles::Suction::Suction(const playeractor::View& player, const SuctionContext& context,
    std::function<std::uint32_t()> random) : player(player), context(context), random(std::move(random)) {}

unsigned particles::Suction::apply(float accelerationScale, float, float maximumSpeed, const Vector& position,
    Vector& velocity, Vector& acceleration, std::uint8_t& captured) {
    if (player.field8A0 != 1 || !player.field8B4) {
        acceleration = {};
        captured = 0;
        return 0;
    }
    if (!context.position) throw std::runtime_error("Original particle suction position is unavailable.");
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - (*context.position)[i];
    distance = magnitude(delta);
    if (distance <= 20) {
        captured = 0;
        return 2;
    }
    if (player.field8B4->intersects(5.5, direction, position)) captured = 1;
    else {
        if (!retainCapture || !captured) {
            acceleration = {};
            captured = 0;
            return 0;
        }
        direction = normalized(delta);
        for (auto& component : direction) component = -component;
    }
    force(accelerationScale, maximumSpeed, position, velocity, acceleration);
    return 1;
}

void particles::Suction::force(float accelerationScale, float maximumSpeed, const Vector& position,
    Vector& velocity, Vector& acceleration) {
    if (!context.direction || !random) throw std::runtime_error("Original particle suction force dependencies are unavailable.");
    const float accelerationMagnitude = (context.strength / distance) * accelerationScale;
    const float velocityMagnitude = 20.0f * context.strength;
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - (*context.position)[i];
    const auto& axis = *context.direction;
    const float projection = std::fma(delta[0], axis[0], delta[1] * axis[1]) + delta[2] * axis[2];
    Vector transverse;
    for (unsigned i = 0; i < 3; ++i) transverse[i] = axis[i] * projection - delta[i];
    const float transverseDistance = magnitude(transverse);
    const float threshold = 20.0f * (1.0f + static_cast<float>(random()) / 4294967296.0f);
    const auto forceDirection = transverseDistance <= threshold ? direction : normalized(transverse);
    for (unsigned i = 0; i < 3; ++i) {
        acceleration[i] = forceDirection[i] * accelerationMagnitude + acceleration[i];
        velocity[i] = direction[i] * velocityMagnitude;
    }
    if (maximumSpeed < magnitude(velocity)) {
        velocity = normalized(velocity);
        for (auto& component : velocity) component *= maximumSpeed;
    }
}

particles::SuctionCallback::SuctionCallback(Suction& suction) : suction(suction) {}

bool particles::SuctionCallback::advance(Emitter&, Node& node) { return advance(node); }

bool particles::SuctionCallback::advance(Node& node) const {
    const auto previous = node.fields54[4];
    auto captured = previous;
    Vector position, velocity, acceleration;
    for (unsigned i = 0; i < 3; ++i) {
        float offset;
        std::memcpy(&offset, &node.fields24[i], sizeof(offset));
        position[i] = node.fields0[i] + offset;
        velocity[i] = node.fields0[i + 3];
        acceleration[i] = node.fields0[i + 6];
    }
    const auto result = suction.apply(accelerationScale, field8, maximumSpeed, position, velocity, acceleration, captured);
    for (unsigned i = 0; i < 3; ++i) {
        node.fields0[i + 3] = velocity[i];
        node.fields0[i + 6] = acceleration[i];
    }
    if (capture == 1 && previous != 1) node.fields54[4] = captured;
    if (result == 1) {
        if (capture == 1) --node.fields30[1];
    } else if (result == 0) {
        if (previous == 1) for (unsigned i = 6; i < 9; ++i) node.fields0[i] = 0;
    } else if (result == 2) node.fields30[1] = node.fields30[0];
    return true;
}
