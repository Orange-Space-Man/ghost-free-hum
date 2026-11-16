#include "game/enemies/enemyghostcaptureparent.h"

#include <stdexcept>

std::uint32_t enemystrategy::GhostCaptureParent::fishingBegin(characterevent::Event& event) {
    captureLatch = true;
    const bool replacingActive = capture && active == capture.get();
    capture.reset();
    if (replacingActive) active = nullptr;
    capture = createCapture();
    if (!capture) throw std::runtime_error("Ghost capture requires its original strategy selector.");
    capture->owner = owner;
    capture->context = context;
    capture->setNextState(0);
    if (!active) active = capture.get();
    setNextState(7);
    lightState = 0;
    if (state == 5) notifyAttackFishing(event);
    return 1;
}

std::uint32_t enemystrategy::GhostCaptureParent::captureBegin(characterevent::Event&) {
    return 1;
}

bool enemystrategy::GhostCaptureParent::query(enemies::Owner&) {
    return lightState != 0;
}

void enemystrategy::GhostCaptureParent::initializeBehavior() {
    switch (state) {
    case 7:
        captureLatch = false;
        break;
    case 10:
        disappearance().begin();
        break;
    case 11:
        setNextState(10);
        break;
    default:
        initializeGhostState();
        break;
    }
}

void enemystrategy::GhostCaptureParent::behavior() {
    switch (state) {
    case 7:
        if (result == 0x102) setNextState(11);
        break;
    case 10:
        disappearance().update();
        break;
    case 11:
        break;
    default:
        updateGhostState();
        break;
    }
    updateKnockback();
}

void enemystrategy::GhostCaptureParent::completeDisappearance() {
    if (!owner) throw std::runtime_error("Ghost disappearance has no character owner.");
    finishSecondary();
    captureLatch = false;
    clearElement();
    auto& character = dynamic_cast<characterowner::Owner&>(*owner);
    auto& actor = dynamic_cast<actors::MoveActor&>(character.actor());
    actor.flags |= 0xc000;
    if (actor.selectAnimation("NONE")) {
        character.state().field8cc = 0;
        character.state().field8d0 = 0;
    }
    character.event(8);
}
