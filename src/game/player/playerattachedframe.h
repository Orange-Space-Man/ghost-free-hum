#pragma once

#include "game/player/playeractor.h"

namespace playercomponents {
    class AttachedFrame {
    public:
        explicit AttachedFrame(const std::vector<std::uint8_t>& executable);
        AttachedInput build(playeractor::Player& player) const;
        AttachedOutput run(playeractor::Player& player, const actormodel::Catalog& catalog,
            const actormodel::Resources& resources, const actormodel::InitialPose& pose) const;

    private:
        std::array<float, 3> constants;
        float scannerHorizontal;
    };
}
