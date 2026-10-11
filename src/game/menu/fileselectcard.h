#pragma once

#include "game/menu/fileselectstate.h"
#include "game/save/gamecardstorage.h"

namespace fileselect {
    bool writeCardAction(const Request& request, gamesave::State& saves, const gamesave::State& defaults,
        gamecard::Storage& card, const gamecard::Block& banner, unsigned tick, unsigned modified, std::string& error);
}
