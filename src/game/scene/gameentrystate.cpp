#include "game/scene/gameentrystate.h"
#include <algorithm>

bool gameentry::selectNewFile(const gamesave::State& saves, unsigned slot, State& state, std::string& error) {
    error.clear();
    if (!saves.initialized || slot >= 3 || gamesave::slotUsed(saves, slot)) {
        error = "New game requires an initialized empty save slot.";
        return false;
    }
    const auto begin = saves.working.begin() + 12 + slot * 0x3b0;
    if (begin[0x3a7] != 0) {
        error = "Completed-file restart initialization is not ported yet.";
        return false;
    }
    State result;
    result.slot = slot;
    std::copy_n(begin + 2, result.flags.size(), result.flags.begin());
    result.flags.back() &= 0x1f;
    std::copy_n(begin + 0x14, result.inventory.size(), result.inventory.begin());
    std::copy_n(begin + 0x234, result.player.size(), result.player.begin());
    std::copy_n(begin + 0x3a0, result.record.size(), result.record.begin());
    result.cutscene = (result.flags[0] & 0x80) ? 0 : 0x4c;
    result.initialized = true;
    state = result;
    return true;
}
