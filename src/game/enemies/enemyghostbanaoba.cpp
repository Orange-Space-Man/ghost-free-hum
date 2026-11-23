#include "game/enemies/enemyghostbanaoba.h"
#include <stdexcept>
#include <utility>

ghostparent::BanaobaAttack::BanaobaAttack(BanaobaServices value) : services(std::move(value)) {
    if (!services.birth || !services.createBanana)
        throw std::runtime_error("EnAtkBanaoba requires its animation timing and enemy creation services.");
}

void ghostparent::BanaobaAttack::initializeBehavior() {
    if (state == 1) {
        // 800DF2CC / 800DF37C: state 1 begins the EAT animation.
        if (services.actor.selectAnimation("EAT")) {
            services.character.field8cc = 0;
            services.character.field8d0 = 0;
        }
    }
}

void ghostparent::BanaobaAttack::behavior() {
    if (state == 0) setNextState(1);
    else if (state == 1) {
        // 800DF2FC: the birth timing track, rather than a fixed timer, releases the peel.
        if (services.birth() >= 0.5f) services.createBanana();
        if (services.character.field8d0 & 1) setNextState(0x100);
    }
}
