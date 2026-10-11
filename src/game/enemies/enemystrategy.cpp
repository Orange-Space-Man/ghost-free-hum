#include "game/enemies/enemystrategy.h"
#include "game/enemies/enemystrategydecorator.h"
#include "game/enemies/enemynomovestrategy.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

void enemystrategy::Strategy::begin() {
}

void enemystrategy::Strategy::update() {
    if (nextState != 0xffff) {
        changeState();
    }
    behavior();
    ++frames;
}

bool enemystrategy::Strategy::query(enemies::Owner&) {
    return false;
}

void enemystrategy::Strategy::finish() {
}

void enemystrategy::Strategy::behavior() {
}

void enemystrategy::Strategy::initializeBehavior() {
}

void enemystrategy::Strategy::setNextState(std::uint16_t value) {
    nextState = value;
}

void enemystrategy::Strategy::changeState() {
    state = nextState;
    nextState = 0xffff;
    frames = 0;
    initializeBehavior();
}


void enemystrategy::Decorator::update() {
    if (!active) {
        Strategy::update();
    } else {
        active->update();
        if (active->state > 0xff && active->state < 0x103) {
            result = active->state;
            active = nullptr;
        }
    }
}


void enemystrategy::NoMove::behavior() {
    if (!owner) throw std::runtime_error("Original EnNoMove strategy has no character owner.");
    dynamic_cast<actors::MoveActor&>(owner->actor()).setVelocity({0, 0, 0});
}
