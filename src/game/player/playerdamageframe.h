#pragma once

#include "game/player/playerliferecovery.h"
#include "game/player/playereventpayload.h"
#include "game/actors/actorregistry.h"

namespace playerevents {
    class DamageFrame {
    public:
        explicit DamageFrame(const std::vector<std::uint8_t>& executable);
        bool run(playeractor::Player& player, const Payload& payload, bool alternateReaction,
            const actors::Registry& actors, const actormodel::Catalog& catalog, bool doubledDamage,
            LifeEnvironment& life) const;

    private:
        std::int32_t ordinaryDelay, reactionDelay;
    };
}
