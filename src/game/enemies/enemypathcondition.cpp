#include "game/enemies/enemypathcondition.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

namespace {
    characterowner::Owner& character(enemies::Owner* owner) {
        if (!owner) throw std::runtime_error("Original path condition has no character owner.");
        return dynamic_cast<characterowner::Owner&>(*owner);
    }
}

enemystrategy::PathCondition::PathCondition(const mission::Resources& resources) : resources(resources) {}

void enemystrategy::PathCondition::initializeBehavior() {
    character(owner).loadPath(resources);
}

void enemystrategy::PathCondition::behavior() {
    auto& parent = character(owner);
    auto& path = parent.path();
    if (!path.attached()) return;
    auto& manager = parent.manager();
    if (!path.count()) {
        const auto& actor = dynamic_cast<actors::MoveActor&>(parent.actor());
        if (manager.nearPlayer(actor.position, 200) != 0) setNextState(0x100);
        return;
    }
    for (unsigned row = 0; row < path.count(); ++row) {
        std::array<float, 3> position{-320000, -320000, -320000};
        path.getValue(row, "pnt0_x", position[0]);
        path.getValue(row, "pnt0_y", position[1]);
        path.getValue(row, "pnt0_z", position[2]);
        if (manager.nearPlayer(position, 200) >= 0) {
            setNextState(0x100);
            return;
        }
    }
}
