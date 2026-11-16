#include "game/enemies/enemyghostchakasu.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
    constexpr float floatingHeight = 130.0f;
    constexpr float reapproachExtra = 0.2f;
    constexpr unsigned teaseFrames = 6;

    bool startAnimation(actors::MoveActor& actor, characterowner::State& character, const char* name, std::uint32_t count) {
        const bool selected = actor.selectAnimation(name);
        if (selected) {
            character.field8cc = count;
            character.field8d0 = 0;
        }
        return selected;
    }
}

ghostparent::ChakasuAttack::ChakasuAttack(ChakasuServices value) : services(std::move(value)) {
    if (!services.player || !services.frame)
        throw std::runtime_error("Original Chakasu attack requires its player and simulation-clock services.");
}

void ghostparent::ChakasuAttack::initializeBehavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    if (state == 1) {
        startAnimation(actor, character, "OU", 0);
        tracked = &services.player(character.field950);
    } else if (state == 2) {
        startAnimation(actor, character, "TYAKASU", teaseFrames);
    }
}

void ghostparent::ChakasuAttack::behavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    if (state == 0) {
        setNextState(1);
    } else if (state == 1) {
        if (!tracked) throw std::runtime_error("Original Chakasu approach has no bound player.");
        const std::array<float, 3> delta{tracked->position[0] - actor.position[0], tracked->position[1] - actor.position[1],
            tracked->position[2] - actor.position[2]};
        const float distance = std::sqrt((delta[0] * delta[0] + delta[1] * delta[1]) + delta[2] * delta[2]);
        const auto angle = services.pose.angle(delta[2], delta[0]);
        if (threshold <= distance) {
            const auto& parameters = character.fields93c;
            auto velocity = ghostmotion::horizontal(services.pose, actor.velocity, parameters[0], parameters[2], angle);
            const float floor = services.collision.floorHeight(actor.position);
            velocity[1] = ghostmotion::vertical(services.pose, parameters[1], parameters[3], actor.position[1], floor, 0.0f,
                floatingHeight, actor.id(), services.frame(), 0x800);
            actor.setVelocity(velocity);
            actor.previousRotation[1] = static_cast<std::int16_t>(angle);
        } else {
            setNextState(2);
        }
    } else if (state == 2) {
        std::array<float, 3> delta{};
        faceTarget(actor, services.pose, services.player(character.field950).position, &delta);
        actor.setVelocity({0, 0, 0});
        const float horizontal = std::sqrt(delta[0] * delta[0] + delta[2] * delta[2]);
        if (!(character.field8d0 & 1)) {
            if (static_cast<double>(threshold * (1.0f + reapproachExtra)) < static_cast<double>(horizontal)) setNextState(1);
        } else {
            setNextState(0x100);
        }
    }
}
