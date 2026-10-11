#pragma once

#include "game/player/playeractor.h"
#include "game/events/runtimeflags.h"

namespace playerevents {
    bool collectKey(playeractor::Player& player, std::uint8_t door, gameflags::State& flags);
}
