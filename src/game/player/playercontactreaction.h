#pragma once

#include "game/player/playerliferecovery.h"
#include "game/world/mapcollisionstate.h"

namespace playeractor {
    class ContactReaction {
    public:
        explicit ContactReaction(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const mapcollision::State& collision, const gameplaycamera::CameraOwner& camera,
            bool doubledDamage, playerevents::LifeEnvironment& life) const;

    private:
        float threshold, height, distance, feedback, turning;
    };
}
