#pragma once

#include "game/player/playerdoorinteraction.h"
#include "game/player/playerdamageframe.h"
#include "game/player/playeractioncontext.h"

namespace playerinteraction {
    struct DoorFrameServices {
        DoorServices& interaction;
        const actors::Registry& actors;
        gameflags::State& flags;
        playerevents::LifeEnvironment& life;
        bool doubledDamage = false;
        std::function<void(unsigned)> globalSound;
    };

    class DoorFrame {
    public:
        explicit DoorFrame(const std::vector<std::uint8_t>& executable);
        DoorInput input(const playeractor::Player& player) const;
        bool run(playeractor::Player& player, DoorFrameServices& services) const;

    private:
        DoorInteraction interaction;
        playeractions::Context actions;
        playerevents::DamageFrame damage;
        float approachSpeed, turnBlend;
        std::uint32_t retryDelay;
    };
}
