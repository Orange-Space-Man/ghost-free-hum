#pragma once

#include "game/player/playerliferecovery.h"

namespace playeractor {
    struct TimerServices {
        playerevents::LifeEnvironment& life;
        bool doubledDamage;
        unsigned frame;
        std::function<void(bool)> changeAudioRate;
        std::function<void(Player&, const std::array<float, 3>&, std::uint32_t)> delayedDamage;
        std::function<void(Player&, std::uint32_t, unsigned, unsigned, unsigned)> weaponPulse;
        std::function<void(unsigned, const actormodel::Matrix&)> matrixEffect;
    };

    class TimerFrame {
    public:
        explicit TimerFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const TimerServices& services) const;

    private:
        std::int32_t powerDelay, escapePulse, escapeDelay;
        float escapeDecay;
        std::uint32_t pulseColor;
    };
}
