#include "game/enemies/enemyappearancestrategy.h"
#include "game/actors/actortimingsampler.h"
#include <stdexcept>
#include <utility>

std::array<unsigned, 2> enemystrategy::appearanceEffects(unsigned type) {
    return type == 13 || type == 175 ? std::array<unsigned, 2>{0x16, 0x17} :
        std::array<unsigned, 2>{0x18, 0x19};
}

enemystrategy::Appearance::Appearance(AppearanceServices value) : services(std::move(value)) {
    if (!services.notifyPlayer || !services.projectFloor || !services.particle || !services.playerPosition)
        throw std::runtime_error("Original ghost appearance requires its player, floor and particle services.");
}

void enemystrategy::Appearance::initializeBehavior() {
    if (state != 1) return;
    auto& actor = services.actor;
    if (actor.selectAnimation("APPEAR2")) {
        services.character.field8cc = 0;
        services.character.field8d0 = 0;
    }
    services.notifyPlayer();
    auto position = actor.position;
    services.projectFloor(position);
    for (const auto effect : appearanceEffects(actor.type())) services.particle(effect, position);
    actor.flags &= 0xffff3bff;
}

void enemystrategy::Appearance::behavior() {
    if (state == 0) {
        setNextState(1);
        return;
    }
    if (state != 1) return;
    auto& actor = services.actor;
    actor.setVelocity({0, 0, 0});
    const auto model = services.catalog.type(actor.type()).model;
    const auto timing = services.models.timing(model, actor.keyIndex & 0xffff);
    const auto alpha = actoranimation::timingAlpha(
        actoranimation::sampleTiming(timing.get(), "alfa1_timing", actor.animationFrame));
    actor.fieldE0 = (actor.fieldE0 & 0xffffff00) | alpha;
    actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | alpha;
    const auto player = services.playerPosition();
    actor.previousRotation[1] = static_cast<std::int16_t>(services.pose.angle(
        player[2] - actor.position[2], player[0] - actor.position[0]));
    if (services.character.field8d0 & 1) setNextState(0x102);
}
