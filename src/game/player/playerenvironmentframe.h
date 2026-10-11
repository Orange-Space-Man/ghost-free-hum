#pragma once

#include "game/player/playeractor.h"
#include "game/world/mapcollisionstate.h"

#include <functional>

namespace playerenvironment {
    struct Services {
        const mapcollision::State& collision;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        const actormodel::InitialPose& pose;
        enemies::Manager& enemies;
        const playermodels::VacuumEntries& capture;
        std::function<void(playeractor::Player&)> refreshType;
        std::function<playeractor::Target*(const playeractor::Player&)> captureTarget;
        std::function<bool(unsigned)> roomDark;
        std::function<void(bool)> captureSound;
        std::function<void(unsigned)> roomSound;
    };

    class Frame {
    public:
        explicit Frame(const std::vector<std::uint8_t>& executable);
        void run(playeractor::Player& player, const Services& services) const;

    private:
        std::array<float, 25> constants;
        actorcollision::Volume inverted;
    };
}
