#pragma once

#include "game/player/playeractor.h"
#include "game/player/playervacuumeffects.h"

namespace playeractor {
    class EnergyFrame {
    public:
        explicit EnergyFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const actormodel::Catalog& catalog, bool vacuumUpdated) const;
        bool beginCaptureReaction(Player& player, const playermodels::VacuumEntries& entries,
            std::uint8_t& previousReaction) const;

    private:
        std::int32_t maximum, recovery;
        float idleStrength, zero, step, costCategory, costFraction;
    };
}
