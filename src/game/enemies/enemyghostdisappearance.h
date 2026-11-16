#pragma once

#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include <functional>

namespace enemystrategy {
    struct DisappearanceServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        std::function<void(std::array<float, 3>&)> projectFloor;
        std::function<void(unsigned, const std::array<float, 3>&)> particle;
        std::function<void()> releaseShared;
        std::function<void(void*&)> releaseReference;
        std::function<void()> complete;
    };

    class GhostDisappearance {
    public:
        explicit GhostDisappearance(DisappearanceServices services);
        void begin();
        void update();

    private:
        DisappearanceServices services;
    };
}
