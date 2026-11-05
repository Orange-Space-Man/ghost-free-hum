#pragma once

#include "game/player/playerlatestate.h"
#include "game/player/playercaptureframe.h"

namespace playeractor {
    struct LateFrameServices {
        bool hidePlayers = false;
        std::function<void(Player&, bool)> scannerTransition;
        std::function<void(Player&, float, bool, bool, bool)> pipe;
        std::function<void(Player&)> sheets;
    };

    class LateFrame {
    public:
        explicit LateFrame(const std::vector<std::uint8_t>& executable);
        void begin(Player& player, const LateFrameServices& services) const;
        void finish(Player& player, const playermodels::SharedModels& shared,
            const LateFrameServices& services) const;

    private:
        LateState state;
        CaptureFrame capture;
    };
}
