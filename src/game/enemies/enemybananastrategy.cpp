#include "game/enemies/enemybananastrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/actorcollisionvolume.h"
#include "game/player/playereventdispatch.h"
#include "game/player/playeractor.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void load(gameparameters::Parameters& parameters, const gamearchive::Archive& archive, const char* path) {
        const int entry = archive.findPathResource(path);
        if (entry < 0) return;
        std::string error;
        if (!parameters.load(archive.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
    }
    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

enemystrategy::Banana::Banana(ItemEnvironment& value) : environment(value),
    physics(value.collision, value.surfaces, value.suction.pose) {
    // 800E0684: defaults for /param/th/EnBanana.prm, in the original parameter order.
    parameters.add("mDamage", 4, 0);
    parameters.add("mAttackType", 2, 0);
    parameters.add("mIgnoreTime", 4, 60);
    parameters.add("mIgnoreCharaCol", 4, 30);
    parameters.add("mLife", 4, 1800);
    parameters.add("mLifeCollided", 4, 60);
    parameters.add("mBlinkFeq", 4, 2);
    parameters.add("mBlinkLife", 4, 50);
    parameters.add("mTailColor", 4, 0xfff200ff);
    parameters.add("mTailLife", 4, 30);
    parameters.add("mSlipPow", 4, 0x41200000);
    load(parameters, value.common, "/param/th/EnBanana.prm");
    load(physics.parameters, value.common, "/param/th/EnThrowUp_banana.prm");
}

actors::MoveActor& enemystrategy::Banana::actor() const {
    if (!owner) throw std::runtime_error("EnBanana requires its character owner.");
    return dynamic_cast<actors::MoveActor&>(owner->actor());
}

void enemystrategy::Banana::begin() {
    // 800DF998: a placed peel has a finite lifetime; one thrown by Banaoba does not.
    placed = owner->source.table && owner->source.row >= 0;
    remaining = placed ? parameters.entries()[4].bits : 0;
    blink = 0;
}

void enemystrategy::Banana::selectAnimation(const char* name) {
    if (actor().selectAnimation(name)) {
        auto& character = dynamic_cast<characterowner::Owner&>(*owner).state();
        character.field8cc = 0;
        character.field8d0 = 0;
    }
}

void enemystrategy::Banana::initializeBehavior() {
    auto& object = actor();
    if (state == 0) {
        // 800DFA1C: initialize the throw physics, yellow trail and WAIT animation.
        physics.owner = owner;
        physics.context = context;
        physics.setNextState(0);
        trail.initialize(&object, {0, 0}, {{{30, 30, 0}, {-30, -30, 0}}}, 2,
            parameters.entries()[8].bits, static_cast<std::uint8_t>(parameters.entries()[9].bits), 0, 0);
        selectAnimation("WAIT");
    } else if (state == 1) {
        // 800DFBA8: the peel is active once the initial throw has begun.
        physics.setNextState(0);
        grounded = false;
        trail.reset();
    } else if (state == 2) {
        // 800DFCF0: launch along player zero's direction, blended toward up.
        const auto id = environment.collection.enemies.appearPoint(0).actor;
        auto* source = environment.collection.actors.actor(id);
        if (!source) throw std::runtime_error("EnBanana requires player zero for its collision launch.");
        auto velocity = dynamic_cast<playeractor::Player&>(*source).state.direction;
        const float length = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
        if (length > 0.0000038146973f) for (auto& axis : velocity) axis /= length;
        else velocity = {};
        const float scale = number(parameters.entries()[10].bits);
        velocity[0] = velocity[0] * 0.25f * scale;
        velocity[1] = (velocity[1] * 0.25f + 0.75f) * scale;
        velocity[2] = velocity[2] * 0.25f * scale;
        object.setVelocity(velocity);
        physics.setNextState(0);
        grounded = false;
    } else if (state == 3) {
        // 800DFEA4 fades the actor's target alpha at the ordinary four-step rate.
        object.fieldE4 &= 0xffffff00u;
        object.fieldE8 = 4;
    }
}

void enemystrategy::Banana::remove() {
    characterevent::Event event(4);
    event.fields[4] = 0;
    owner->event(event);
}

void enemystrategy::Banana::behavior() {
    auto& object = actor();
    if (state == 0) { setNextState(1); return; }
    if (state == 1) {
        // 800DFF24 / 800DFBF0: the peel settles before it can hit Luigi.
        physics.update();
        if (frames > static_cast<std::uint32_t>(parameters.entries()[3].bits) && (object.flags & 0x10000)) {
            grounded = true;
            object.setRotation({0, 0, 0});
        }
        if (environment.trails) trail.update(environment.trails);
        // 800DFBF0 selects CLOSE when the secondary contact probe reports a hit;
        // the retail banana model has WAIT and CLOSE (OPEN is an absent lookup).
        selectAnimation(grounded ? "CLOSE" : "OPEN");
        if (placed) {
            if (remaining) --remaining;
            if (remaining < parameters.entries()[7].bits) {
                if (++blink >= parameters.entries()[6].bits) {
                    blink = 0;
                    object.flags |= 0xc000;
                } else object.flags &= ~0xc000u;
            }
            if (!remaining) remove();
        }
    } else if (state == 2) {
        physics.update();
        if (frames >= parameters.entries()[5].bits) setNextState(3);
    } else if (state == 3) {
        physics.update();
        if ((object.fieldE0 & 255) == 0) remove();
    }
}

std::uint32_t enemystrategy::Banana::collideWithPlayer(characterevent::Event& event) {
    // 800DF628: only a settled peel can send the common attack event.
    if (state != 1 || !grounded || frames < parameters.entries()[3].bits) return 0;
    if (!environment.events) throw std::runtime_error("EnBanana requires player damage dispatch.");
    constexpr std::uint32_t attackKinds[]{6, 7, 5};
    const auto attackType = parameters.entries()[1].bits;
    if (attackType >= 3) throw std::runtime_error("EnBanana attack type is out of range.");
    playerevents::Payload payload;
    payload.word(0x14, actor().id());
    payload.word(0x18, parameters.entries()[0].bits);
    payload.bytes[0x1c] = 1;
    payload.word(0x20, attackKinds[attackType]);
    environment.events->send(static_cast<std::int32_t>(event.fields[1]), 3, &payload);
    setNextState(2);
    return 1;
}
