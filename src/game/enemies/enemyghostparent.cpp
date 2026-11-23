#include "game/enemies/enemyghostparent.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    constexpr std::uint16_t lightFinished = 0x100;
    constexpr std::uint16_t lightResult = 0x101;
}

ghostparent::Parent::Parent(ParentServices value) {
    bind(std::move(value));
}

ghostparent::Parent::Parent() = default;

void ghostparent::Parent::bind(ParentServices value) {
    if (!value.random || !value.place || !value.assemble || !value.roomCleared || !value.elementEffect || !value.createCapture ||
        !value.notifyAttackFishing || !value.finishSecondary || !value.clearElement || !value.lightResult101 ||
        !value.secondResult101 || !value.damagePlayer)
        throw std::runtime_error("Original ghost parent requires its child, room, effect, damage and capture services.");
    bound.emplace(std::move(value));
}

ghostparent::ParentServices& ghostparent::Parent::services() const {
    if (!bound) throw std::runtime_error("Original ghost parent used before its owner services were bound.");
    return const_cast<ParentServices&>(*bound);
}
bool ghostparent::Parent::hitAdmissible() const {
    return state == 5 || state == 8 || state == 9;
}

void ghostparent::Parent::setLightDelegate(enemystrategy::Strategy* delegate) {
    lightDelegate = delegate;
    lightState = delegate ? 1 : 0;
}

std::unique_ptr<enemystrategy::Strategy> ghostparent::Parent::createCapture() { return services().createCapture(); }
void ghostparent::Parent::notifyAttackFishing(characterevent::Event& event) { services().notifyAttackFishing(event); }
void ghostparent::Parent::updateKnockback() { ghostparent::updateKnockback(fields, services().actor); }
enemystrategy::GhostDisappearance& ghostparent::Parent::disappearance() { return services().disappearance; }
void ghostparent::Parent::finishSecondary() { services().finishSecondary(); }
void ghostparent::Parent::clearElement() { services().clearElement(); }

enemystrategy::Strategy& ghostparent::Parent::child(enemystrategy::Strategy* value, const char* name) const {
    if (!value) throw std::runtime_error(std::string("Original ghost parent has no ") + name + " child.");
    return *value;
}

bool ghostparent::Parent::pumpLight() {
    if (!lightState) lightDelegate = nullptr; // fishing (800C7A0C) clears +0x590 through the base class
    if (!lightDelegate) return false;
    lightDelegate->update();
    const auto result = lightDelegate->state;
    if (result == lightResult) services().lightResult101();
    else if (result == lightFinished) setLightDelegate(nullptr);
    return true;
}

bool ghostparent::Parent::pumpSecond() {
    if (!secondDelegate) return false;
    secondDelegate->update();
    const auto result = secondDelegate->state;
    if (result == 0x101) {
        secondDelegate = nullptr;
        services().secondResult101();
    } else if (result == 0x100) {
        secondDelegate = nullptr;
        setNextState(state);
    }
    return true;
}

void ghostparent::Parent::faceFirstPlayer() {
    faceTarget(services().actor, services().pose, services().manager.playerPosition(0));
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
        if (services().actor.fieldB4 == services().manager.playerRoom(0)) setNextState(2);
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
    // Event 8 restarts the parent after an uncaptured light fade. The previous EnLight
    // delegate belongs to that attempt and must not suppress the new sight search.
    setLightDelegate(nullptr);
    const InitializeServices initialization{services().actor, services().character, services().parameters, services().source, services().random};
    selection = initialize(fields, initialization);
    if (!fields.holding && (selection.configuration.place == 1 || selection.configuration.place == 2))
        services().place(selection.configuration.place, fields.placementId);
    children = services().assemble(selection);
}

void ghostparent::Parent::enterWait() {
    auto& actor = services().actor;
    auto& character = services().character;
    if (fields.invisibleMode == 1) {
        character.fields93c[2] = randomOffset(speedForMode(services().parameters, 1), services().random);
        character.field94e = 0;
        actor.visibility(0, 0);
        actor.flags |= 0xc400;
    } else {
        character.fields93c[2] = randomOffset(speedForMode(services().parameters, 0), services().random);
        character.field94e = static_cast<std::uint8_t>(services().random() % 105 + 150);
    }
    actor.visibility(character.field94e, character.field94e);
}

void ghostparent::Parent::enterMovement() {
    auto& character = services().character;
    if (services().actor.selectAnimation("NONE")) {
        character.field8cc = 0;
        character.field8d0 = 0;
    }
    child(children.movement, "movement").setNextState(0);
}

void ghostparent::Parent::updateMovement() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services().character, services().actor);
    if (!fields.holding) child(children.movement, "movement").update();
    else faceFirstPlayer();
    const int seen = sightQuery(services().manager, services().actor, parameterWord(services().parameters, Parameter::Eyesight));
    if (seen >= 0 || fields.overrides.movement == 6) setNextState(3);
}

void ghostparent::Parent::enterSearch() {
    child(children.movement, "movement").setNextState(0);
    child(children.condition, "condition").setNextState(0);
}

void ghostparent::Parent::updateSearch() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services().character, services().actor);
    auto& search = child(children.search, "search");
    auto& condition = child(children.condition, "condition");
    if (!fields.holding) search.update();
    else faceFirstPlayer();
    condition.update();
    const bool cleared = services().roomCleared(services().actor.fieldB4 >> 24);
    if (!cleared && (search.state == 0x100 || condition.state == 0x100)) {
        fields.field5A2 = 1;
        if (fields.invisibleMode == 1) setNextState(4);
        else if (fields.invisibleMode == 0) setNextState(5);
    }
}

void ghostparent::Parent::enterAppearance() {
    child(children.appearance, "appearance").setNextState(0);
    if (!fields.skipElementEffect) services().elementEffect(fields.element);
    services().character.field94e = static_cast<std::uint8_t>(maximumAlpha(fields));
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
    services().character.fields93c[2] = randomOffset(speedForMode(services().parameters, 2), services().random);
    child(children.attack, "attack").setNextState(0);
}

void ghostparent::Parent::updateAttack() {
    if (pumpLight() || pumpSecond()) return;
    updateFade(fields, services().character, services().actor);
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

void ghostparent::Parent::pushAway() {
    auto& actor = services().actor;
    std::array<float, 3> delta{};
    faceTarget(actor, services().pose, services().manager.playerPosition(services().character.field950), &delta);
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

void ghostparent::Parent::enterKnockback() {
    pushAway();
}
void ghostparent::Parent::updateKnockbackState() {
    auto& actor = services().actor;
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
    auto& actor = services().actor;
    auto& character = services().character;
    std::array<float, 3> displacement{};
    sightQuery(services().manager, actor, parameterWord(services().parameters, Parameter::Eyesight), &displacement);
    actor.previousRotation[1] = static_cast<std::int16_t>(services().pose.angle(displacement[2], displacement[0]));
    if (parameterWord(services().parameters, count) < 1) {
        afterAttack(*this, parameterWord(services().parameters, action));
    } else if (actor.selectAnimation(animation)) {
        // 800C8714 / 800C8824 retain the parameter in r5 for 800BFB98.
        // Zero disables the owner's loop-completion timer.
        character.field8cc = static_cast<std::uint32_t>(parameterWord(services().parameters, count));
        character.field8d0 = 0;
    }
}

void ghostparent::Parent::updateAfterAttack(Parameter action) {
    services().actor.setVelocity({0, 0, 0});
    if (pumpLight() || pumpSecond()) return;
    if (services().character.field8d0 & 1) afterAttack(*this, parameterWord(services().parameters, action));
}

void ghostparent::Parent::startLight() {
    auto* light = services().light ? services().light() : nullptr;
    if (!light) throw std::runtime_error("Original ghost light delegate requires its EnLight strategy.");
    light->owner = owner;
    light->context = context;
    light->setNextState(0);
    setLightDelegate(light);
}

std::uint32_t ghostparent::Parent::enteredFlashlight(characterevent::Event& event) {
    if (!lightState) lightDelegate = nullptr;
    // 800C7824: refused while faded (+0x5A8), delegated (+0x594), not hit-admissible or capturing (+0x5A0).
    if (fields.fadeScaled != 0 || secondDelegate || !hitAdmissible() || captureLatch) return 0;
    if (lightDelegate) return 1;
    if (state == 4) {
        auto* observer = dynamic_cast<characterevent::Observer*>(children.appearance);
        if (observer && (observer->enteredFlashlight(event) & 0xff) == 0) return 0;
    }
    startLight();
    if (state == 5) {
        if (auto* observer = dynamic_cast<characterevent::Observer*>(children.attack)) observer->enteredFlashlight(event);
    }
    return 1;
}

std::uint32_t ghostparent::Parent::collideWithPlayer(characterevent::Event& event) {
    // 800C74C4
    if (!lightState) lightDelegate = nullptr;
    if (lightDelegate || captureLatch) return 0;
    if (state != 5) {
        if (state < 5 ? state > 3 : state == 10) return 0;
        services().damagePlayer(event.fields[1]);
        setNextState(6);
        return 1;
    }
    auto* observer = dynamic_cast<characterevent::Observer*>(children.attack);
    if (!observer) return 0;
    const auto result = observer->collideWithPlayer(event);
    if ((result & 0xff) == 0) {
        // 800C8C40: the same push as state 6 plus the retained base velocity and thirty knockback frames.
        pushAway();
        fields.knockbackBase = fields.knockback;
        fields.knockbackFrames = 30;
    }
    return result;
}

std::uint32_t ghostparent::Parent::playerLeftRoom(characterevent::Event& event) {
    // 800C7AAC
    if (state == 5) {
        if (auto* observer = dynamic_cast<characterevent::Observer*>(children.attack)) observer->playerLeftRoom(event);
    }
    completeDisappearance();
    return 1;
}

std::uint32_t ghostparent::Parent::playerEnteredRoom(characterevent::Event&) {
    return 1;
}

std::uint32_t ghostparent::Parent::replacement(characterevent::Event& event) {
    // 800C7974: the hold (+0x5A1) ends unless the placement row names an access (800C90D4: its access_name column is
    // present and differs from "(null)") and the search has not fired (+0x5A2); the attack child then sees the event.
    bool access = false;
    std::string_view accessName;
    if (owner && owner->source.table && owner->source.row >= 0 &&
        owner->source.table->getValue(owner->source.row, "access_name", accessName))
        access = accessName != "(null)";
    if (!access || fields.field5A2) fields.holding = false;
    if (state == 5) {
        if (auto* observer = dynamic_cast<characterevent::Observer*>(children.attack)) observer->replacement(event);
    }
    return 1;
}

std::uint32_t ghostparent::Parent::sprayedWithFire(characterevent::Event&) {
    throw std::runtime_error("Original ghost element reaction (800C777C via 800C75C0) is not ported.");
}

std::uint32_t ghostparent::Parent::sprayedWithIce(characterevent::Event&) {
    throw std::runtime_error("Original ghost element reaction (800C77B4 via 800C75C0) is not ported.");
}

std::uint32_t ghostparent::Parent::sprayedWithWater(characterevent::Event&) {
    throw std::runtime_error("Original ghost element reaction (800C77EC via 800C75C0) is not ported.");
}
