#pragma once

#include "game/actors/actorregistry.h"

namespace playerevents {
    const std::array<float, 3>* resolvePosition(const actors::Registry& actors,
        const std::array<float, 3>* position, std::int32_t actor);
    bool beginAttraction(playeractor::Player& player, const actors::Registry& actors,
        const std::array<float, 3>* position, std::int32_t actor, std::int32_t duration);
}
