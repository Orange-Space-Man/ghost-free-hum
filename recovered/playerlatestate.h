#pragma once

#include "game/player/playeractor.h"

namespace playeractor {
    struct LateStateServices {
        bool hidePlayers = false;
        std::function<void(Player&, bool)> scannerTransition;
    };

    class LateState {
    public:
        explicit LateState(const std::vector<std::uint8_t>& executable);
        void begin(Player& player, const LateStateServices& services) const;
        void scanner(playermodels::ScannerModels& owner, const playermodels::SharedModels& shared) const;

    private:
        float distanceNear, distanceFar, scannerDistance, warpVertical, warpDistance;
        std::array<float, 3> scannerScale, warpScale;
    };
}
