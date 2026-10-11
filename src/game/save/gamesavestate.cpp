#include "game/save/gamesavestate.h"

#include <algorithm>

namespace {
    void write16(std::array<std::uint8_t, 0x2000>& data, std::size_t offset, unsigned value) {
        data[offset] = static_cast<std::uint8_t>(value >> 8);
        data[offset + 1] = static_cast<std::uint8_t>(value);
    }
}

bool gamesave::initialize(const std::vector<std::uint8_t>& initialFlags, State& state, std::string& error) {
    state = {};
    error.clear();
    const auto end = std::find(initialFlags.begin(), initialFlags.end(), 0xff);
    if (end == initialFlags.end()) {
        error = "Missing original save flag terminator.";
        return false;
    }
    State result;
    write16(result.working, 10, 0x3b0);
    for (unsigned slot = 0; slot < 3; ++slot) {
        const std::size_t start = 12 + slot * 0x3b0;
        const auto inventory = start + 0x14;
        for (auto flag = initialFlags.begin(); flag != end; ++flag) {
            result.working[inventory + *flag / 8] |= static_cast<std::uint8_t>(0x80 >> (*flag & 7));
        }
        result.working[inventory + 0x12d] |= 2;
        const auto player = start + 0x234;
        for (unsigned index = 0; index < 25; ++index) write16(result.working, player + index * 2, 100);
        result.working[player + 0xd3] = 1;
        result.working[player + 0x148] = 0x20;
        result.working[player + 0x16a] = 1;
    }
    result.working[0xb1c] = 1;
    result.working[0xb1d] = 1;
    result.committed = result.working;
    result.initialized = true;
    state = result;
    return true;
}

bool gamesave::slotUsed(const State& state, unsigned slot) {
    return state.initialized && slot < 3 && (state.working[12 + slot * 0x3b0 + 1] & 1) != 0;
}

unsigned gamesave::soundMode(const State& state) { return state.working[0xb1c]; }
bool gamesave::rumbleEnabled(const State& state) { return state.working[0xb1d] != 0; }
