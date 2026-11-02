#pragma once

#include "playerfurniturescancache.h"
#include "game/player/playeractor.h"
#include "game/world/mapcollisionstate.h"

namespace playerinteraction {
    struct FurnitureServices {
        playerscanner::FurnitureCache& cache;
        roomfurniture::Owner& furniture;
        const roomlocation::Rooms& rooms;
        enemies::Manager& enemies;
        const actormodel::Catalog& catalog;
        const actorcollision::Manager& collision;
        const mapcollision::State& map;
        unsigned mapMode;
        std::function<void(const playerscanner::SheetInput&)> initializeSheet;
        std::function<std::uint32_t()> random;
    };

    class FurnitureFrame {
    public:
        explicit FurnitureFrame(const std::vector<std::uint8_t>& executable);
        void run(playeractor::Player& player, FurnitureServices& services) const;
        bool actor(playeractor::Player& player, FurnitureServices& services) const;
        bool wall(playeractor::Player& player, const mapcollision::State& collision) const;

    private:
        float reachRatio, collisionScale, wallLimit, turnBlend;
        unsigned actorEvent;
    };
}
