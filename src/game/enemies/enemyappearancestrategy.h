#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include <functional>

namespace enemystrategy {
    std::array<unsigned, 2> appearanceEffects(unsigned type);

    struct AppearanceServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        const actormodel::InitialPose& pose;
        std::function<void()> notifyPlayer;
        std::function<void(std::array<float, 3>&)> projectFloor;
        std::function<void(unsigned, const std::array<float, 3>&)> particle;
        std::function<std::array<float, 3>()> playerPosition;
    };

    class Appearance final : public Strategy {
    public:
        explicit Appearance(AppearanceServices services);
        void initializeBehavior() override;
        void behavior() override;

    private:
        AppearanceServices services;
    };
}
