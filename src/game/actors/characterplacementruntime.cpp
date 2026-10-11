#include "game/actors/characterplacementruntime.h"

#include <stdexcept>

characterplacement::Runtime::Runtime(enemies::Manager& manager, enemies::Runtime& actors,
    const roomdependencies::Catalog& dependencies) : manager(manager), actors(actors), dependencies(dependencies) {
}

unsigned characterplacement::Runtime::resolveRoom(const std::array<float, 3>& position) {
    return actors.resolveRoom(position);
}

bool characterplacement::Runtime::roomVisible(unsigned room) {
    if (!manager.actorsInitialized()) throw std::runtime_error("Original placement visibility requires initialized players.");
    for (unsigned player = 0; player < manager.playerCount(); ++player) {
        if (dependencies.contains(actors.playerRoom(player), room)) return true;
    }
    return false;
}
