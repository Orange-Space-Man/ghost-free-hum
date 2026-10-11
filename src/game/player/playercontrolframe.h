#pragma once

#include "game/player/playerliferecovery.h"

namespace playercontrols {
    class Frame {
    public:
        explicit Frame(const std::vector<std::uint8_t>& executable);
        void charge(playeractor::Player& player, const playerevents::LifeEnvironment& environment) const;
        void attack(playeractor::Player& player) const;
        void blink(playeractor::Player& player, unsigned frame) const;
        void special(playeractor::Player& player, unsigned frame, bool doubledDamage, std::uint8_t& specialActive,
            playerevents::LifeEnvironment& environment) const;

    private:
        float attackDuration;
        std::int32_t decayAmount, decayDelay;
    };
}
