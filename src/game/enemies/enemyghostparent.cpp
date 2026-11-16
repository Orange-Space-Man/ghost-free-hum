#include "game/enemies/enemyghostparent.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    constexpr std::uint16_t lightFinished = 0x100;
    constexpr std::uint16_t lightResult = 0x101;
}

ghostparent::Parent::Parent(ParentServices value) : services(std::move(value)) {
    if (!services.random || !services.assemble || !services.roomCleared || !services.elementEffect || !services.createCapture ||
        !services.notifyAttackFishing || !services.finishSecondary || !services.clearElement || !services.lightResult101 ||
        !services.secondResult101)
        throw std::runtime_error("Original ghost parent requires its child, room, effect and capture services.");
}

bool ghostparent::Parent::hitAdmissible() const {
    return state == 5 || state == 8 || state == 9;
}

void ghostparent::Parent::setLightDelegate(enemystrategy::Strategy* delegate) {
    lightDelegate = delegate;
    lightState = delegate ? 1 : 0;
}

std::unique_ptr<enemystrategy::Strategy> ghostparent::Parent::createCapture() { return services.createCapture(); }
void ghostparent::Parent::notifyAttackFishing(characterevent::Event& event) { services.notifyAttackFishing(event); }
void ghostparent::Parent::updateKnockback() { ghostparent::updateKnockback(fields, services.actor); }
enemystrategy::GhostDisappearance& ghostparent::Parent::disappearance() { return services.disappearance; }
void ghostparent::Parent::finishSecondary() { services.finishSecondary(); }
void ghostparent::Parent::clearElement() { services.clearElement(); }

enemystrategy::Strategy& ghostparent::Parent::child(enemystrategy::Strategy* value, const char* name) const {
    if (!value) throw std::runtime_error(std::string("Original ghost parent has no ") + name + " child.");
    return *value;
}

bool ghostparent::Parent::pumpLight() {
    if (!lightDelegate) return false;
    lightDelegate->update();
    const auto result = lightDelegate->state;
    if (result == lightResult) services.lightResult101();
    else if (result == lightFinished) setLightDelegate(nullptr);
    return true;
}

bool ghostparent::Parent::pumpSecond() {
    if (!secondDelegate) return false;
    secondDelegate->update();
    const auto result = secondDelegate->state;
    if (result == 0x101) {
        secondDelegate = nullptr;
        services.secondResult101();
    } else if (result == 0x100) {
        secondDelegate = nullptr;
        setNextState(state);
    }
    return true;
}

void ghostparent::Parent::faceFirstPlayer() {
    faceTarget(services.actor, services.pose, services.manager.playerPosition(0));
}

void ghostparent::Parent::initializeGhostState() {
    switch (state) {
    case 0: enterInitialize(); break;
    case 1: enterWait(); break;
    case 2: enterMovement(); break;
    case 3: enterSearch(); break;
    case 4: enterAppearance(); break;
    case 5: enterAttack(); break;
    case 6: enterKnockback(); break;
    case 8: enterAfterAttack(Parameter::AttackKarakai, Parameter::ActAfterAttackSuccess, "TYAKASU"); break;
    case 9: enterAfterAttack(Parameter::AttackOrooro, Parameter::ActAfterAttackFailure, "ORORO"); break;
    default: throw std::runtime_error("Original ghost parent state " + std::to_string(state) + " is not ported.");
    }
}

void ghostparent::Parent::updateGhostState() {
    switch (state) {
    case 0: setNextState(1); break;
    case 1:
        if (services.actor.fieldB4 == services.manager.playerRoom(0)) setNextState(2);
        break;
    case 2: updateMovement(); break;
    case 3: updateSearch(); break;
    case 4: updateAppearance(); break;
    case 5: updateAttack(); break;
    case 6: updateKnockbackState(); break;
    case 8: updateAfterAttack(Parameter::ActAfterAttackSuccess); break;
    case 9: updateAfterAttack(Parameter::ActAfterAttackFailure); break;
    default: throw std::runtime_error("Original ghost parent state " + std::to_string(state) + " is not ported.");
    }
}

void ghostparent::Parent::enterInitialize() {
    const InitializeServices initialization{services.actor, services.character, services.parameters, services.source, services.random};
    selection = initialize(fields, initialization);
    children = services.assemble(selection);
}

void ghostparent::Parent::enterWait() {
    auto& actor = services.actor;
    auto& character = services.character;
    if (fields.invisibleMode == 1) {
        character.fields93c[2] = randomOffset(speedForMode(services.parameters, 1), services.random);
        character.field94e = 0;
        actor.visibility(0, 0);
        actor.flags |= 0xc400;
    } else {
        character.fields93c[2] = randomOffset(speedForMode(services.parameters, 0), services.random);
        character.field94e = static_cast<std::uint8_t>(services.random() % 105 + 150);
    }
    actor.visibility(character.field94e, character.field94e);
}

void ghostparent::Parent::enterMovement() {
    auto& character = services.character;
    if (services.actor.selectAnimation("NONE")) {
        character.field8cc = 0;
        character.field8d0 = 0;
    }
    child(children.movement, "movement").setNextState(0);
}

void ghostparent::Parent::updateMovement() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services.character, services.actor);
    if (!fields.holding) child(children.movement, "movement").update();
    else faceFirstPlayer();
    const int seen = sightQuery(services.manager, services.actor, parameterWord(services.parameters, Parameter::Eyesight));
    if (seen >= 0 || fields.overrides.movement == 6) setNextState(3);
}

void ghostparent::Parent::enterSearch() {
    child(children.movement, "movement").setNextState(0);
    child(children.condition, "condition").setNextState(0);
}

void ghostparent::Parent::updateSearch() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services.character, services.actor);
    auto& search = child(children.search, "search");
    auto& condition = child(children.condition, "condition");
    if (!fields.holding) search.update();
    else faceFirstPlayer();
    condition.update();
    const bool cleared = services.roomCleared(services.actor.fieldB4 >> 24);
    if (!cleared && (search.state == 0x100 || condition.state == 0x100)) {
        fields.field5A2 = 1;
        if (fields.invisibleMode == 1) setNextState(4);
        else if (fields.invisibleMode == 0) setNextState(5);
    }
}

void ghostparent::Parent::enterAppearance() {
    child(children.appearance, "appearance").setNextState(0);
    if (!fields.skipElementEffect) services.elementEffect(fields.element);
    services.character.field94e = static_cast<std::uint8_t>(maximumAlpha(fields));
}

void ghostparent::Parent::updateAppearance() {
    if (pumpLight() || pumpSecond()) return;
    auto& appearance = child(children.appearance, "appearance");
    appearance.update();
    const auto result = appearance.state;
    if (result == 0x102) setNextState(5);
    else if (result < 0x102 && result > 0xff) setNextState(10);
}

void ghostparent::Parent::enterAttack() {
    services.character.fields93c[2] = randomOffset(speedForMode(services.parameters, 2), services.random);
    child(children.attack, "attack").setNextState(0);
}

void ghostparent::Parent::updateAttack() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services.character, services.actor);
    auto& attack = child(children.attack, "attack");
    attack.update();
    const auto result = attack.state;
    if (result == 0x101) setNextState(9);
    else if (result < 0x101) {
        if (result > 0xff) setNextState(8);
    } else if (result < 0x103) {
        setNextState(10);
        if (fields.invisibleMode == 1) setNextState(10);
        else if (fields.invisibleMode == 0) setNextState(1);
    }
}

void ghostparent::Parent::enterKnockback() {
    auto& actor = services.actor;
    std::array<float, 3> delta{};
    faceTarget(actor, services.pose, services.manager.playerPosition(services.character.field950), &delta);
    const float squared = (delta[0] * delta[0] + delta[1] * delta[1]) + delta[2] * delta[2];
    if (3.8146973e-6f < squared) {
        const double factor = 1.0f * reciprocalSquareRoot(static_cast<double>(squared));
        delta[0] = static_cast<float>(static_cast<double>(delta[0]) * static_cast<double>(static_cast<float>(factor)));
        delta[2] = static_cast<float>(static_cast<double>(delta[2]) * static_cast<double>(static_cast<float>(factor)));
    } else {
        delta[0] = delta[2] = 0.0f;
    }
    delta[1] = 0.0f;
    for (auto& component : delta) component *= -30.0f;
    fields.knockback = delta;
    actor.setVelocity(delta);
}

void ghostparent::Parent::updateKnockbackState() {
    auto& actor = services.actor;
    const std::array<float, 3> velocity = actor.velocity;
    std::array<float, 3> remaining = fields.knockback;
    for (auto& component : fields.knockback) component *= 0.62f;
    std::array<float, 3> scaled = velocity;
    ghostparent::scale(scaled, 0.25);
    for (unsigned i = 0; i < 3; ++i) remaining[i] -= scaled[i];
    actor.setVelocity({velocity[0] + remaining[0], velocity[1] + remaining[1], velocity[2] + remaining[2]});
    if (30u < frames) setNextState(5);
}

void ghostparent::Parent::enterAfterAttack(Parameter count, Parameter action, const char* animation) {
    auto& actor = services.actor;
    auto& character = services.character;
    std::array<float, 3> displacement{};
    sightQuery(services.manager, actor, parameterWord(services.parameters, Parameter::Eyesight), &displacement);
    actor.previousRotation[1] = static_cast<std::int16_t>(services.pose.angle(displacement[2], displacement[0]));
    if (parameterWord(services.parameters, count) < 1) {
        afterAttack(*this, parameterWord(services.parameters, action));
    } else if (actor.selectAnimation(animation)) {
        character.field8cc = 0;
        character.field8d0 = 0;
    }
}

void ghostparent::Parent::updateAfterAttack(Parameter action) {
    services.actor.setVelocity({0, 0, 0});
    if (pumpLight() || pumpSecond()) return;
    if (services.character.field8d0 & 1) afterAttack(*this, parameterWord(services.parameters, action));
}
