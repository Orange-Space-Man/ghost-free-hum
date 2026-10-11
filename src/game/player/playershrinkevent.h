#pragma once

#include "game/player/playerliferecovery.h"

namespace playerevents {
    struct ShrinkEnvironment {
        LifeEnvironment& life;
        const std::int32_t& transitionFrames;
        float& audioScale;
    };

    unsigned shrink(playeractor::Player& player, float ratio, std::uint32_t frames, ShrinkEnvironment& environment);
}
