#include "game/enemies/enemyghostdisappearance.h"
#include "game/actors/actortimingsampler.h"

#include <stdexcept>
#include <utility>

enemystrategy::GhostDisappearance::GhostDisappearance(DisappearanceServices services) : services(std::move(services)) {
    if (!this->services.projectFloor || !this->services.particle || !this->services.releaseShared ||
        !this->services.releaseReference || !this->services.complete)
        throw std::invalid_argument("Ghost disappearance requires original effect, ownership and completion services.");
}

void enemystrategy::GhostDisappearance::begin() {
    auto& actor = services.actor;
    auto& character = services.character;
    if (actor.selectAnimation("KIERU")) {
        character.field8cc = 0;
        character.field8d0 = 0;
    }
    actor.flags |= 0x400;
    auto position = actor.position;
    services.projectFloor(position);
    services.particle(7, position);
    services.releaseShared();
    if (character.actorUsers[0]) {
        services.releaseReference(character.actorUsers[0]);
        character.flags904[2] = false;
    }
    const float step = static_cast<float>(actor.fieldE0 & 255) / 30.0f;
    const auto rounded = static_cast<std::uint8_t>(static_cast<int>(step < 0 ? step - 0.5f : step + 0.5f));
    actor.fieldE4 &= 0xffffff00;
    actor.fieldE8 = rounded ? rounded : 4;
}

void enemystrategy::GhostDisappearance::update() {
    auto& actor = services.actor;
    actor.setVelocity({0, 0, 0});
    const auto model = services.catalog.type(actor.type()).model;
    const auto timing = services.models.timing(model, actor.keyIndex & 0xffff);
    const auto alpha = actoranimation::timingAlpha(
        actoranimation::sampleTiming(timing.get(), "alfa2_timing", actor.animationFrame));
    actor.fieldE0 = (actor.fieldE0 & 0xffffff00) | alpha;
    actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | alpha;
    if (services.character.field8d0 & 1) services.complete();
}
