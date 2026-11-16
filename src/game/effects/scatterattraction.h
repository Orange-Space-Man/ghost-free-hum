#pragma once

#include "billscatterpool.h"
#include "game/effects/particlescenecontext.h"

namespace scattraction {
    int apply(billscatter::Particle& particle, unsigned mode, const particles::SceneContext& context,
        float range, std::uint32_t& gameplayRandom);
}
