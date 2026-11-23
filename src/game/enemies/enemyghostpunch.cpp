#include "game/enemies/enemyghostpunch.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
    // Original SDA constants used by EnAtkPunch (804D3B20..804D3B34).
    constexpr float punchVelocityScale = 0.8f;
    constexpr unsigned turnFrames = 30;
    constexpr float approachHeight = 225.0f;
    constexpr float strikeRange = 150.0f;
    constexpr float reapproachExtra = 0.2f;
    constexpr float floatingHeight = 130.0f;
}

ghostparent::PunchAttack::PunchAttack(PunchServices value) : services(std::move(value)) {
    if (!services.player || !services.frame || !services.damageNormal || !services.damagePunch || !services.attackActive)
        throw std::runtime_error("Original EnAtkPunch requires its player, clock, timing and damage services.");
}

void ghostparent::PunchAttack::animation(const char* name, unsigned duration) {
    if (services.actor.selectAnimation(name)) {
        services.character.field8cc = duration;
        services.character.field8d0 = 0;
    }
}

std::array<float, 3> ghostparent::PunchAttack::playerDelta() const {
    const auto& player = services.player(services.character.field950);
    return {player.position[0] - services.actor.position[0], player.position[1] - services.actor.position[1],
        player.position[2] - services.actor.position[2]};
}

void ghostparent::PunchAttack::follow(const std::array<float, 3>& delta, float speedScale) {
    auto& actor = services.actor;
    const auto& speed = services.character.fields93c;
    const auto angle = services.pose.angle(delta[2], delta[0]);
    auto velocity = ghostmotion::horizontal(services.pose, actor.velocity, speed[0], speed[2] * speedScale, angle);
    const float floor = services.collision.floorHeight(actor.position);
    velocity[1] = ghostmotion::vertical(services.pose, speed[1], speed[3], actor.position[1], floor, 0.0f,
        floatingHeight, actor.id(), services.frame(), 0x800);
    actor.setVelocity(velocity);
    actor.previousRotation[1] = static_cast<std::int16_t>(angle);
}

void ghostparent::PunchAttack::initializeBehavior() {
    auto& actor = services.actor;
    cycle = 0;
    switch (state) {
    case 0: break;
    case 1:
        // 800DE244: OU starts the approach to the owner's selected player.
        animation("OU", 0);
        break;
    case 2:
        // 800DE3AC: the lunge pose precedes the close-range attack.
        animation("KAMAERU2", 0);
        break;
    case 3:
        animation("KAMAERU2", 0);
        break;
    case 4:
        // 800DE6BC: the contact phase starts the KOGEKI animation.
        animation("KOGEKI", 0);
        touched = false;
        break;
    }
    if (state == 4) actor.setVelocity({0, 0, 0});
}

void ghostparent::PunchAttack::behavior() {
    auto& actor = services.actor;
    const auto delta = playerDelta();
    const float range = std::sqrt(delta[0] * delta[0] + delta[2] * delta[2]);
    switch (state) {
    case 0: setNextState(1); break;
    case 1:
        // 800DE2B8: approach the player until the ghost is within its 225-unit approach height.
        follow(delta, 1.0f);
        if (range <= approachHeight) setNextState(2);
        break;
    case 2:
        // 800DE3E0: track the player at half the normal top speed; attack only inside 150 units.
        follow(delta, 0.5f);
        if (range < strikeRange && std::abs(delta[1]) < strikeRange) setNextState(4);
        else if (range > approachHeight * (1.0f + reapproachExtra)) setNextState(1);
        break;
    case 3:
        // 800DE618: turn during the original thirty-frame recovery interval.
        faceTarget(actor, services.pose, services.player(services.character.field950).position);
        actor.setVelocity({0, 0, 0});
        if (++cycle >= turnFrames) setNextState(1);
        break;
    case 4:
        // 800DE728: the punch resolves when the selected KOGEKI animation completes.
        follow(delta, 0.5f);
        auto velocity = actor.velocity;
        for (auto& component : velocity) component *= punchVelocityScale;
        actor.setVelocity(velocity);
        if (services.character.field8d0 & 1) setNextState(touched ? 0x100 : 0x101);
        break;
    }
}

std::uint32_t ghostparent::PunchAttack::collideWithPlayer(characterevent::Event& event) {
    // 800DE184 gates body damage by the original animation's "attack" timing track.
    if (services.attackActive()) {
        if (!touched) services.damagePunch(event.fields[1]);
        touched = true;
        return 1;
    }
    services.damageNormal(event.fields[1]);
    return 0;
}
