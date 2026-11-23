#pragma once

#include "moneyscatterpool.h"
#include "game/effects/particlesuction.h"
#include "game/player/playeractorview.h"

namespace moneyscatter {
    int attract(const playeractor::View& player, const particles::SuctionContext& context,
        float radius, Particle& particle);
}
