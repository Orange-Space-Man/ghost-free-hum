#pragma once

#include "game/player/playeractor.h"
#include "game/player/playerorientationupdate.h"

namespace playerorientation {
    struct FrameSignals {
        bool attachedTurn;
        float damping, scannerDamping;
        unsigned frame;
    };

    class Frame {
    public:
        Frame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose);
        Input build(playeractor::Player& player, const FrameSignals& signals) const;
        void run(playeractor::Player& player, const FrameSignals& signals) const;

    private:
        const actormodel::InitialPose& pose;
        std::array<float, 33> constants;
        float completion;
    };
}
