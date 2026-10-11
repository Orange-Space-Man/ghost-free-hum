#include "game/enemies/enemybombstrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/actorcollisionvolume.h"
#include "game/scene/gameplaycameraowner.h"
#include "game/audio/jaudiopositionowners.h"
#include "game/player/playerbombdamage.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float number(std::uint32_t bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::uint32_t bits(float value) {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
}

enemystrategy::Bomb::Bomb(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {
    auto& parameters = environment.bombParameters;
    if (parameters.entries().empty()) {
        parameters.add("sCallSoundY", 4, 0x40800000);
        parameters.add("sIgnoreColFrame", 4, 0x41200000);
        parameters.add("sOmega", 2, 0x800);
        parameters.add("sFramePika", 4, 30);
        parameters.add("sFrameJibaku", 4, 60);
        parameters.add("sFramePika2", 4, 10);
        parameters.add("sFrameJibaku2", 4, 40);
        parameters.add("sFrameJibakuPlus", 4, 20);
        parameters.add("sFrameJibakuEnd", 4, 32);
        parameters.add("sDistJibaku", 4, 0x41700000);
    }
    const auto entry = environment.common.findPathResource("/param/th/AEnItemBomb.prm");
    std::string error;
    if (entry >= 0 && !parameters.load(environment.common.resource(static_cast<unsigned>(entry)), error))
        throw std::runtime_error(error);
}

enemystrategy::Bomb::~Bomb() { releaseEmitters(); }

void enemystrategy::Bomb::releaseEmitters() {
    for (auto*& emitter : emitters) {
        if (emitter) emitter->release();
        emitter = nullptr;
    }
}

void enemystrategy::Bomb::begin() {
    baseScale = actor().type() == 0xd0 ? 0.7f : 1.0f;
    actor().scale.fill(baseScale);
    actor().selectTexture(0, 0);
}

itemsuction::Vector enemystrategy::Bomb::fusePosition() const {
    const auto& joint = actor().model->matrix(0, 1);
    const auto& matrix = actor().matrix;
    itemsuction::Vector result;
    for (unsigned i = 0; i < 3; ++i) result[i] =
        ((matrix[i * 4] * joint[3] + matrix[i * 4 + 1] * joint[7]) + matrix[i * 4 + 2] * joint[11]) + matrix[i * 4 + 3];
    return result;
}

void enemystrategy::Bomb::initializeItem() {
    flags &= ~3u;
    if (!owner->source.table || owner->source.row < 0) flags |= 0x14;
    const auto position = fusePosition();
    emitters[0] = environment.suction.effects.create(0x53, position, {});
    emitters[1] = environment.suction.effects.create(actor().type() == 0xd0 ? 0x52 : 0x51, position, {});
    dynamic_cast<characterowner::Owner&>(*owner).state().field950 = 0;
    phase = 0;
    spin = static_cast<std::int16_t>(environment.bombParameters.entries()[2].bits);
    suction.completionKind = 1;
    suction.completionValue = actor().hitDamage();
    volume.field8 = 1;
    volume.secondary = actorcollision::Record{bits(1), bits(1), 0, bits(-20), 0};
}

void enemystrategy::Bomb::beginActive() { beginFinite(); }

void enemystrategy::Bomb::animation(const char* name) {
    if (actor().selectAnimation(name)) {
        dynamic_cast<characterowner::Owner&>(*owner).state().field8cc = 0;
        dynamic_cast<characterowner::Owner&>(*owner).state().field8d0 = 0;
    }
}

void enemystrategy::Bomb::remove() {
    characterevent::Event event(4);
    event.fields[4] = 0;
    owner->event(event);
}

void enemystrategy::Bomb::explode() {
    if (!environment.camera || !environment.audioPositions) throw std::runtime_error("Original bomb camera and positional audio owners are not bound.");
    environment.suction.effects.createExplosion(actor().type() == 0xd0 ? 2 : 1, actor().position);
    environment.sounds.play(0x18b0, actor().position, 0);
    environment.audioPositions->release(&actor().position);
    environment.camera->fieldA8 = 100;
}

void enemystrategy::Bomb::damagePlayer() {
    if (!environment.life || !environment.mansionMode) throw std::runtime_error("Original bomb damage services are not bound.");
    playerevents::bombDamage(player(dynamic_cast<characterowner::Owner&>(*owner).state().field950), actor().hitDamage(), *environment.mansionMode, *environment.life);
}

void enemystrategy::Bomb::updateActive() {
    physics.update();
    auto& object = actor();
    const auto& parameters = environment.bombParameters.entries();
    if (physics.state == 2) {
        if (std::fabs(object.velocity[1]) <= number(parameters[0].bits)) spin = 0;
        else environment.sounds.play(0x18c8, object.position, 0);
        ++groundedFrames;
    }
    if (static_cast<float>(frames) <= number(parameters[1].bits)) {
        if (!owner->source.table || owner->source.row < 0) animation("NOMAPCOL");
    } else animation("NORMAL");
    updateFinite(false);
    object.scale.fill(baseScale);
    object.previousRotation[0] = static_cast<std::int16_t>(std::uint16_t(spin) + std::uint16_t(object.rotation[2]));
    if (!phase) {
        auto position = fusePosition();
        for (unsigned i = 0; i < 3; ++i) position[i] += object.velocity[i];
        for (auto* emitter : emitters) if (emitter) emitter->setAttachmentPosition(position, {});
        const unsigned offset = object.type() == 0xd0 ? 3 : 5;
        const auto start = parameters[offset].bits;
        object.selectTexture(frames < start ? 0 : static_cast<std::int16_t>((frames - start) & 1), 0);
        if (frames == parameters[offset + 1].bits) phase = 1;
    }
    if (phase == 1) {
        radius = 0;
        animation("NORMAL");
        object.visibility(0, 0);
        object.flags |= 0xc400;
        explode();
        releaseEmitters();
    }
    if (static_cast<std::int32_t>(phase) <= 0) return;
    object.setVelocity({});
    if (static_cast<std::int32_t>(phase) < static_cast<std::int32_t>(parameters[7].bits)) {
        radius += 0.55f * (baseScale * number(parameters[9].bits));
    }
    volume.secondary = actorcollision::Record{bits(radius), bits(radius), 0, bits(-20 - 0.2f * radius), 0};
    const auto& view = player(dynamic_cast<characterowner::Owner&>(*owner).state().field950).view;
    actorcollision::Vector displacement{};
    if (!view.fieldF15 && actorcollision::intersect(displacement, volume, object.position, *view.volume, *view.position)) {
        damagePlayer();
        remove();
        return;
    }
    if (static_cast<std::int32_t>(parameters[8].bits) < static_cast<std::int32_t>(phase)) remove();
    else ++phase;
}

void enemystrategy::Bomb::onCollected() {
    if (player(dynamic_cast<characterowner::Owner&>(*owner).state().field950).view.fieldF15) return;
    explode();
    damagePlayer();
}

std::uint32_t enemystrategy::Bomb::captureEnd(characterevent::Event& event) {
    FiniteItem::captureEnd(event);
    actor().selectTexture(0, 0);
    return 1;
}

std::uint32_t enemystrategy::Bomb::sprayedWithFire(characterevent::Event&) {
    if (!phase) phase = 1;
    return 1;
}

std::uint32_t enemystrategy::Bomb::sprayedWithWater(characterevent::Event&) {
    environment.suction.effects.create(2, actor().position, {});
    remove();
    return 1;
}
