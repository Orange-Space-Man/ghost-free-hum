#pragma once

#include "game/player/playerliferecovery.h"

namespace playerevents {
    bool bombDamage(playeractor::Player& player, std::uint32_t damage, std::uint32_t mansionMode, LifeEnvironment& environment);
}
