#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gamesave {
    struct State {
        std::array<std::uint8_t, 0x2000> working{};
        std::array<std::uint8_t, 0x2000> committed{};
        bool initialized = false;
    };

    bool initialize(const std::vector<std::uint8_t>& initialFlags, State& state, std::string& error);
    bool slotUsed(const State& state, unsigned slot);
    unsigned soundMode(const State& state);
    bool rumbleEnabled(const State& state);
}
