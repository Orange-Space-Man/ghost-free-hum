#include "game/enemies/enemyreplacestrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

namespace {
    characterowner::Owner& character(enemies::Owner* owner) {
        if (!owner) throw std::runtime_error("Original EnReplace strategy has no character owner.");
        return dynamic_cast<characterowner::Owner&>(*owner);
    }
}

void enemystrategy::Replace::initializeBehavior() {
    if (state != 0) return;
    auto& actor = dynamic_cast<actors::MoveActor&>(character(owner).actor());
    actor.flags |= 0xc400;
    actor.visibility(0, 0);
}

void enemystrategy::Replace::behavior() {
    if (state != 0 && state != 1) return;
    auto& parent = character(owner);
    auto& actor = dynamic_cast<actors::MoveActor&>(parent.actor());
    auto& manager = parent.manager();
    if (state == 0) {
        actor.setVelocity({0, 0, 0});
        if (frames >= 60) {
            if (!manager.restorePlacement(parent)) parent.event(4, nullptr);
            setNextState(1);
        }
    } else {
        const int player = manager.nearPlayer(actor.position, 200);
        const bool visible = manager.roomVisible(actor.resolvePositionRoom());
        if (player < 0 || !visible) setNextState(0x100);
    }
}
