#pragma once

#include "game/enemies/enemyghostparenthelpers.h"
#include "game/enemies/enemyghostmovement.h"

namespace ghostparent {
    struct ChakasuServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const mapcollision::State& collision;
        const actormodel::InitialPose& pose;
        std::function<actors::MoveActor&(unsigned)> player;
        std::function<std::uint32_t()> frame;
    };

    class ChakasuAttack final : public enemystrategy::Strategy {
    public:
        explicit ChakasuAttack(ChakasuServices services);
        void initializeBehavior() override;
        void behavior() override;
        static constexpr float threshold = 300.0f;

    private:
        ChakasuServices services;
        actors::MoveActor* tracked = nullptr;
    };
}
