#include "game/enemies/enemyitemappearance.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

namespace {
    actors::MoveActor& actor(enemystrategy::Strategy& strategy) {
        if (!strategy.owner) throw std::runtime_error("Original item appearance has no character owner.");
        return dynamic_cast<actors::MoveActor&>(strategy.owner->actor());
    }
}

void enemystrategy::ItemImmediateAppearance::behavior() {
    setNextState(0x100);
}

void enemystrategy::ItemFadeAppearance::initializeBehavior() {
    if (state == 0) {
        auto& object = actor(*this);
        object.visibility(0, 0);
        object.fieldE4 = (object.fieldE4 & 0xffffff00) | 255;
        object.fieldE8 = 4;
    }
}

void enemystrategy::ItemFadeAppearance::behavior() {
    if (state == 0) setNextState(1);
    else if (state == 1 && (actor(*this).fieldE0 & 255) == 255) setNextState(0x100);
}
