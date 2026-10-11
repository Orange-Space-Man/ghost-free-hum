#include "game/player/playerkeycollection.h"
#include "game/player/playeractorcomponents.h"

#include <stdexcept>

bool playerevents::collectKey(playeractor::Player& player, std::uint8_t door, gameflags::State& flags) {
    auto& keys = *player.components->flags;
    if (!keys.data || (unsigned(door) >> 3) >= keys.bytes) throw std::runtime_error("Original key event exceeds its bound player storage.");
    auto& value = keys.data[door >> 3];
    const auto mask = static_cast<std::uint8_t>(1u << (door & 7));
    if (value & mask) return false;
    value |= mask;
    switch (door) {
    case 3: flags.setOn(0x27); break;
    case 0x1b: flags.setOn(0x50); break;
    case 0x21: flags.setOn(0x0c); break;
    case 0x2a: flags.setOn(0x44); break;
    case 0x3b: flags.setOn(0x52); break;
    }
    player.state.field1061 = 1;
    player.state.field1068 = door;
    return true;
}
