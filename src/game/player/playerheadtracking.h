#pragma once

#include "game/actors/actorregistry.h"

namespace playercomponents {
    class HeadFrame {
    public:
        explicit HeadFrame(const std::vector<std::uint8_t>& executable);
        void run(playeractor::Player& player, const actors::Registry& actors, const enemies::Manager& manager,
            const actormodel::Catalog& catalog, const actormodel::InitialPose& pose) const;

    private:
        float range, targetHeight, blend;
    };
}
