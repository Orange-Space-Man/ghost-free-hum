#pragma once

#include "game/player/playeractionupdate.h"
#include "game/player/playeractor.h"
#include "game/world/mapcollisionstate.h"

namespace playeractions {
    struct ContextSignals {
        bool ladder;
        bool footTiming;
        float footCrossing;
    };

    class Context {
    public:
        explicit Context(const std::vector<std::uint8_t>& executable);
        Input build(playeractor::Player& player, const ContextSignals& signals) const;
        Input build(playeractor::Player& player, const mapcollision::State& collision,
            const actormodel::Catalog& catalog, const actormodel::Resources& models) const;
        float maximumSpeed(const playeractor::Player& player) const;

    private:
        std::array<float, 3> thresholds;
        std::array<float, 9> speeds;
        std::array<std::int32_t, 4> idleThresholds;
        float turnFactor, reactionFactor;
    };
}
