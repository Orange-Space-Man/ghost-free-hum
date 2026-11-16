#include "game/enemies/enemysearchstrategy.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
    std::array<float, 3> difference(const actors::MoveActor& target, const actors::MoveActor& actor) {
        return {target.position[0] - actor.position[0], target.position[1] - actor.position[1],
            target.position[2] - actor.position[2]};
    }
}

enemystrategy::NearSearch::NearSearch(SearchServices value, float baseRadius) : services(std::move(value)) {
    if (!services.player || !services.movement.random || !services.movement.frame)
        throw std::runtime_error("Original ghost search requires player, shared random and simulation services.");
    const float fraction = static_cast<float>(services.movement.random() % 10000) / 9999.0f;
    radius = static_cast<float>(double(baseRadius) + double(100.0f * (fraction - 0.5f)));
}

float enemystrategy::NearSearch::height() const {
    const auto& movement = services.movement;
    if (movement.source.table && movement.source.row >= 0) {
        std::uint32_t result = 0;
        movement.source.table->getValue(movement.source.row, "floating_height", result);
        return static_cast<float>(result);
    }
    return 0.5f * (movement.collision.ceilingHeight(movement.actor.position) -
        movement.collision.floorHeight(movement.actor.position));
}

void enemystrategy::NearSearch::initializeBehavior() {
    if (state == 0) floatingHeight = height();
    else if (state == 1) tracked = &services.player(services.movement.character.field950);
    else if (state == 2) {
        direction = 1;
        floatingHeight = height();
    }
}

void enemystrategy::NearSearch::vertical(std::array<float, 3>& velocity) const {
    const auto& movement = services.movement;
    const auto& actor = movement.actor;
    const auto& parameters = movement.character.fields93c;
    const float floor = movement.collision.floorHeight(actor.position);
    const float ceiling = floatingHeight < 0 ? movement.collision.ceilingHeight(actor.position) : 0;
    velocity[1] = ghostmotion::vertical(movement.pose, parameters[1], parameters[3], actor.position[1],
        floor, ceiling, floatingHeight, actor.id(), movement.frame(), 0x800);
}

void enemystrategy::NearSearch::turn() {
    auto& movement = services.movement;
    auto& actor = movement.actor;
    const auto delta = difference(services.player(movement.character.field950), actor);
    actor.previousRotation[1] = static_cast<std::int16_t>(movement.pose.angle(delta[2], delta[0]));
    auto velocity = actor.velocity;
    if (actor.fieldB0 != -1) {
        direction = -direction;
        velocity[0] = velocity[2] = 0;
    }
    const float distance = std::sqrt(delta[0] * delta[0] + delta[2] * delta[2]);
    const float radial = distance - radius;
    const float inverse = static_cast<float>(1.0 / double(distance));
    float x = inverse * (delta[0] * radial - direction * (delta[2] * 12.0f)) - velocity[0];
    float z = inverse * (delta[2] * radial + direction * (delta[0] * 12.0f)) - velocity[2];
    const auto& parameters = movement.character.fields93c;
    const float acceleration = std::sqrt(x * x + z * z);
    if (parameters[0] < acceleration) {
        const float scale = parameters[0] / acceleration;
        x *= scale; z *= scale;
    }
    velocity[0] += x; velocity[2] += z;
    const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
    if (parameters[2] < speed) {
        const float scale = parameters[2] / speed;
        velocity[0] *= scale; velocity[2] *= scale;
    }
    vertical(velocity);
    actor.setVelocity(velocity);
}

void enemystrategy::NearSearch::behavior() {
    auto& movement = services.movement;
    auto& actor = movement.actor;
    if (state == 0) setNextState(1);
    else if (state == 1) {
        const auto delta = difference(*tracked, actor);
        const float distance = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
        if (radius <= distance) {
            const auto angle = movement.pose.angle(delta[2], delta[0]);
            const auto& parameters = movement.character.fields93c;
            auto velocity = ghostmotion::horizontal(movement.pose, actor.velocity, parameters[0], parameters[2], angle);
            vertical(velocity);
            actor.setVelocity(velocity);
            actor.previousRotation[1] = static_cast<std::int16_t>(angle);
        } else setNextState(2);
    } else if (state == 2) turn();
}
