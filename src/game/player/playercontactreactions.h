#pragma once

#include "game/player/playerliferecovery.h"
#include "game/world/mapcollisionstate.h"
#include "game/scene/gameplaycameraowner.h"

namespace playeractions {
    class ContactFrame {
    public:
        explicit ContactFrame(const std::vector<std::uint8_t>& executable);
        void run(playeractor::Player& player, const mapcollision::State& collision,
            const gameplaycamera::CameraOwner& camera, playerevents::LifeEnvironment& life, bool doubledDamage,
            const std::function<void(unsigned, const std::array<float, 3>&, const std::array<float, 3>&)>& effect) const;

    private:
        float wallAlignment, impactHeight, frontOffset, turnBlend;
    };
}
