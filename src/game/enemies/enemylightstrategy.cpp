#include "game/enemies/enemylightstrategy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    constexpr unsigned hitEvent = 21;
    constexpr int fadeFrames = 30;
    constexpr float spinAmplitude = 500.0f;
    constexpr float reducedFactor = 0.5f;
    constexpr unsigned playerEffect = 10;
    constexpr unsigned soundId = 0x4823;
    constexpr unsigned floorParticle = 7;
    constexpr std::array<unsigned, 2> childTypes{0xa5, 0xa6};
    constexpr std::array<unsigned, 12> effectClassIds{0x2d, 0x2b, 0x24, 0x25, 0x0a, 0x22, 0x0b, 0x20, 0x21, 0x0e, 0x27, 0x29};

    std::uint32_t wordFromFloat(float value) {
        if (value != value) return 0x80000000u;
        if (value >= 2147483648.0f) return 0x7fffffffu;
        if (value <= -2147483648.0f) return 0x80000000u;
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    }

    void setAlphaTarget(actors::MoveActor& actor, std::uint8_t value, int count) {
        if (count == 0) count = 4;
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | value;
        actor.fieldE8 = static_cast<std::uint8_t>(count);
    }

    actors::MoveActor* findChildByType(actors::MoveActor& actor, unsigned type) {
        for (auto& child : actor.children) {
            if (child.actor && child.actor->type() == type) return child.actor;
        }
        return nullptr;
    }

    void copyParentAlpha(actors::MoveActor& child) {
        if (!child.parent) return;
        const auto alpha = static_cast<std::uint8_t>(child.parent->fieldE0 & 0xff);
        child.visibility(alpha, alpha);
    }

    bool startEffectObject(unsigned id) {
        if (std::find(effectClassIds.begin(), effectClassIds.end(), id) == effectClassIds.end()) return false;
        throw std::runtime_error("Original effect object class " + std::to_string(id) + " is registered but not ported.");
    }
}

void enemystrategy::beginLightEffect(characterowner::Owner& owner, const actors::Registry& registry) {
    auto& character = owner.state();
    if (!character.actorUsers[0]) {
        character.actorUsers[0] = owner.acquireDependentActor(registry);
        // 80171C20 positions an effect object of class 0 at the attachment joint (80097A9C); none is registered.
        startEffectObject(0);
        character.flags904[2] = true;
    }
    auto& shared = *static_cast<actors::MoveActor*>(character.actorUsers[0]);
    shared.selectAnimation("NORMAL");
    shared.visibility(0xff, 0xff);
}

enemystrategy::Light::Light(LightServices value) : services(std::move(value)) {
    if (!services.bindFrame || !services.minBindRange || !services.maxBindRange || !services.playerPosition ||
        !services.scenarioMatches || !services.reducedScale || !services.event || !services.beginEffect ||
        !services.startEffect || !services.releaseReference || !services.projectFloor || !services.particle ||
        !services.sound)
        throw std::runtime_error("Original light strategy requires its effect, ownership, parameter and player services.");
}

void enemystrategy::Light::updateSpin() {
    const auto angle = static_cast<std::uint32_t>(litCount & 0xff) * 0x9000u;
    const auto value = static_cast<std::int16_t>(static_cast<std::int32_t>(
        spinAmplitude * services.trigonometry.sineAngle(angle)));
    services.actor.rotation[2] = value;
    services.actor.previousRotation[2] = value;
}

std::uint32_t enemystrategy::Light::hitThreshold() const {
    const unsigned type = services.actor.type();
    const std::uint32_t limit = services.bindFrame(type);
    if (services.scenarioMatches()) return limit;
    const auto& position = services.actor.position;
    const auto player = services.playerPosition();
    const float dz = position[2] - player[2], dx = position[0] - player[0], dy = position[1] - player[1];
    const float distance = std::sqrt((dz * dz + dx * dx) + dy * dy);
    const float nearest = services.minBindRange(type), farthest = services.maxBindRange(type);
    float clamped = nearest;
    if (nearest <= distance) {
        clamped = distance;
        if (farthest < distance) clamped = farthest;
    }
    const float range = farthest - nearest;
    const float remaining = farthest - clamped;
    const float life = static_cast<float>(static_cast<double>(static_cast<std::int32_t>(limit)));
    const float product = life * remaining;
    float scaled = static_cast<float>(static_cast<double>(product) / static_cast<double>(range));
    if (services.reducedScale()) scaled *= reducedFactor;
    return wordFromFloat(1.0f <= scaled ? scaled : 1.0f);
}

void enemystrategy::Light::releaseOwnerReference() {
    auto& character = services.character;
    if (character.actorUsers[0]) {
        services.releaseReference(character.actorUsers[0]);
        character.flags904[2] = false;
    }
}

void enemystrategy::Light::initializeBehavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    auto& ownership = services.ownership;
    switch (state) {
    case 1:
        actor.animationSpeed = 0.0f;
        services.beginEffect();
        services.startEffect(character.field950, playerEffect, actor.id());
        litCount = 0;
        break;
    case 2:
        actor.animationSpeed = 1.0f;
        releaseOwnerReference();
        break;
    case 3: {
        services.sound(soundId, actor.position);
        releaseOwnerReference();
        if (character.actorUsers[1]) {
            services.releaseReference(character.actorUsers[1]);
            character.field890 = -1;
        }
        auto position = actor.position;
        services.projectFloor(position);
        services.particle(floorParticle, position);
        if (ownership.sharedUsers[0]) ownership.releaseShared(ownership.sharedUsers[0]);
        if (character.field894) character.field894->release();
        character.field894 = nullptr;
        break;
    }
    }
}

void enemystrategy::Light::behavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    switch (state) {
    case 0:
        setNextState(1);
        break;
    case 1: {
        actor.setVelocity({0, 0, 0});
        updateSpin();
        characterevent::Event hit(hitEvent);
        if (services.event(hit) & 0xff) {
            ++litCount;
            frames = 0;
        }
        if (static_cast<std::int32_t>(hitThreshold()) < litCount) setNextState(3);
        const std::uint32_t elapsed = frames;
        if (hitThreshold() < elapsed) setNextState(2);
        break;
    }
    case 2:
        setNextState(0x100);
        actor.setVelocity({0, 0, 0});
        break;
    case 3:
        character.field94e = 0;
        setAlphaTarget(actor, character.field94e, 0xff / fadeFrames + 1);
        for (const unsigned type : childTypes) {
            if (auto* child = findChildByType(actor, type)) copyParentAlpha(*child);
        }
        actor.setVelocity({0, 0, 0});
        if (character.field94e == (actor.fieldE0 & 0xff)) {
            actor.animationSpeed = 1.0f;
            setNextState(0x101);
        }
        break;
    }
}
