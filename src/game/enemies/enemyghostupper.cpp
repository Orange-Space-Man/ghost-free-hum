#include "game/enemies/enemyghostupper.h"
#include "game/actors/actorcollisionvolume.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
    constexpr float approachDistance = 300.0f;
    constexpr float risingOffset = 100.0f;
    constexpr float launchDistance = 100.0f;
    constexpr unsigned aimFrames = 20;
    constexpr unsigned riseTimeout = 60;
    constexpr unsigned lungeTimeout = 60;
    constexpr unsigned recoveryFrames = 20;

    float horizontalDistance(const std::array<float, 3>& delta) {
        return std::sqrt(delta[0] * delta[0] + delta[2] * delta[2]);
    }
}

ghostparent::UpperAttack::UpperAttack(UpperServices value) : services(std::move(value)) {
    if (!services.player || !services.frame || !services.random || !services.damageNormal || !services.damageStrike)
        throw std::runtime_error("Original AEnAtUpper requires its player, clock and damage services.");
}

void ghostparent::UpperAttack::animation(const char* name) {
    if (services.actor.selectAnimation(name)) {
        services.character.field8cc = 0;
        services.character.field8d0 = 0;
    }
}

std::array<float, 3> ghostparent::UpperAttack::playerDelta() const {
    const auto& player = services.player(services.character.field950);
    return {player.position[0] - services.actor.position[0], player.position[1] - services.actor.position[1],
        player.position[2] - services.actor.position[2]};
}

void ghostparent::UpperAttack::approach(const std::array<float, 3>& delta) {
    auto& actor = services.actor;
    const auto& speed = services.character.fields93c;
    const auto angle = services.pose.angle(delta[2], delta[0]);
    auto velocity = ghostmotion::horizontal(services.pose, actor.velocity, speed[0], speed[2], angle);
    velocity[1] = ghostmotion::vertical(services.pose, speed[1], speed[3], actor.position[1],
        services.collision.floorHeight(actor.position), 0.0f, 130.0f, actor.id(), services.frame(), 0x800);
    actor.setVelocity(velocity);
    actor.previousRotation[1] = static_cast<std::int16_t>(angle);
}

bool ghostparent::UpperAttack::flight(float bend, float speed) {
    auto& actor = services.actor;
    const std::array<float, 3> remaining{target[0] - actor.position[0], target[1] - actor.position[1],
        target[2] - actor.position[2]};
    const float distance = horizontalDistance(remaining);
    const bool passed = remaining[0] * heading[0] + remaining[2] * heading[2] <= 0.0f;
    const float signedDistance = passed ? distance : -distance;
    std::array<float, 3> velocity{heading[0], 2.0f * signedDistance * bend, heading[2]};
    const float length = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
    if (length > 0.0f) for (auto& component : velocity) component = component * speed / length;
    actor.setVelocity(velocity);
    return passed;
}

void ghostparent::UpperAttack::initializeBehavior() {
    auto& actor = services.actor;
    if (state == 0) {
        struck = false;
        elapsed = 0;
        trail.initialize(&actor, {15, 15}, {{{-10, 0, 35}, {-10, 0, -35}}},
            0, 0x66b0ffc8u, 10, 5, -0.1f);
        animation("OU");
        setNextState(1);
    } else if (state == 2) {
        const auto& player = services.player(services.character.field950);
        target = player.position;
        const std::array<float, 3> delta{target[0] - actor.position[0], 0.0f, target[2] - actor.position[2]};
        const float distance = horizontalDistance(delta);
        if (distance > 0.0f) {
            heading[0] = delta[0] / distance;
            heading[2] = delta[2] / distance;
        } else {
            heading = services.pose.horizontalDirection(static_cast<std::uint16_t>(actor.rotation[1]));
        }
        target[0] -= heading[0] * launchDistance;
        target[1] += risingOffset;
        target[2] -= heading[2] * launchDistance;
        actor.previousRotation[1] = static_cast<std::int16_t>(services.pose.angle(delta[2], delta[0]));
    } else if (state == 3) {
        animation("KAMAERU2");
    } else if (state == 4) {
        animation("UPPER");
        trail.reset();
    }
}

void ghostparent::UpperAttack::strike(unsigned player) {
    if (struck) return;
    struck = services.damageStrike(player) != 0;
}

bool ghostparent::UpperAttack::touchingPlayer() const {
    // 800DDD5C passes AEnAtUpper's cylinder through the ordinary actor collision
    // dispatcher against the player's current collision volume.
    static const actorcollision::Volume strikeVolume{1,
        actorcollision::Record{0x42c80000u, 0x42c80000u, 0u, 0xc2480000u, 0u}, std::nullopt};
    const auto& player = services.player(services.character.field950);
    const auto& playerVolume = player.field788 ? *player.field788 :
        static_cast<const actorcollision::Volume&>(services.shapes.type(player.actorType));
    actorcollision::Vector separation{};
    return actorcollision::intersect(separation, strikeVolume, services.actor.position,
        playerVolume, player.position);
}

bool ghostparent::UpperAttack::disappear() {
    if (services.disappearFrames == 0 || ++elapsed < services.disappearFrames) return false;
    if (services.random() % 10000u >= 100u) return false;
    setNextState(0x102);
    return true;
}

void ghostparent::UpperAttack::behavior() {
    auto& actor = services.actor;
    if (state == 1) {
        if (disappear()) return;
        const auto delta = playerDelta();
        actor.previousRotation[1] = static_cast<std::int16_t>(services.pose.angle(delta[2], delta[0]));
        const float distance = horizontalDistance(delta);
        if (distance <= approachDistance) setNextState(2);
        else approach(delta);
    } else if (state == 2) {
        if (disappear()) return;
        auto velocity = actor.velocity;
        velocity[0] *= 0.9f;
        velocity[2] *= 0.9f;
        const std::array<float, 3> delta{target[0] - actor.position[0], 0.0f, target[2] - actor.position[2]};
        const float distance = horizontalDistance(delta);
        const float desiredHeight = target[1] + 0.001f * distance * distance;
        const float error = desiredHeight - actor.position[1];
        const bool atHeight = error >= -25.0f && error <= 25.0f;
        if (atHeight) {
            velocity[1] *= 0.5f;
            if (std::abs(velocity[1]) < 1.1920929e-7f) velocity[1] = 0.0f;
        } else {
            velocity[1] += error > 25.0f ? services.character.fields93c[1] : -services.character.fields93c[1];
        }
        if (std::abs(velocity[1]) >= services.character.fields93c[3])
            velocity[1] = std::copysign(services.character.fields93c[3], velocity[1]);
        actor.setVelocity(velocity);
        if (atHeight) setNextState(3);
        else if (frames >= riseTimeout) setNextState(1);
    } else if (state == 3) {
        if (disappear()) return;
        actor.setVelocity({0, 0, 0});
        if (frames >= aimFrames) setNextState(4);
    } else if (state == 4) {
        const bool passed = flight(0.001f, 40.0f);
        if (services.trails) trail.update(services.trails);
        if (!struck) {
            if (touchingPlayer()) strike(services.character.field950);
        }
        if (passed) setNextState(5);
        else if (frames >= lungeTimeout) setNextState(struck ? 0x100 : 0x101);
    } else if (state == 5) {
        flight(0.0005f, std::max(0.0f, 30.0f - static_cast<float>(frames)));
        if (services.trails) trail.update(services.trails);
        if (!struck) {
            if (touchingPlayer()) strike(services.character.field950);
        }
        if (frames >= recoveryFrames) setNextState(struck ? 0x100 : 0x101);
    }
}

std::uint32_t ghostparent::UpperAttack::collideWithPlayer(characterevent::Event& event) {
    if (state <= 3) services.damageNormal(event.fields[1]);
    return 0;
}
