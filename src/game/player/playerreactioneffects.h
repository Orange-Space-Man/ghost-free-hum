#pragma once

#include "game/player/playeractor.h"
#include "game/effects/modeleffectpool.h"

namespace playeractor {
    void updateReactionEffects(Player& player, const actormodel::Catalog& catalog, const actormodel::Resources& resources,
        particles::EffectFactory& particles, modeleffects::Factory& models,
        const std::function<void(unsigned, const std::array<float, 3>&)>& sound);
}
