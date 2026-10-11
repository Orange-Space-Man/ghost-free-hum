#pragma once

#include "game/save/gamesavestate.h"

namespace gameentry {
    struct State {
        unsigned slot = 0;
        unsigned map = 2;
        unsigned cutscene = 0x4c;
        bool initialized = false;
        std::array<std::uint8_t, 11> flags{};
        std::array<std::uint8_t, 0x220> inventory{};
        std::array<std::uint8_t, 0x16c> player{};
        std::array<std::uint8_t, 16> record{};
    };

    bool selectNewFile(const gamesave::State& saves, unsigned slot, State& state, std::string& error);
}
