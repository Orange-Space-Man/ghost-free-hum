#pragma once

#include "game/player/playerliferecovery.h"

namespace playeractions {
    void advanceState(Reactions& state, Actions* actions, bool enabled, bool silent,
        const std::array<float, 3>& position, const std::function<void(unsigned, const std::array<float, 3>&)>& sound);
    void updateState(playeractor::Player& player, playerevents::LifeEnvironment& environment);
}
