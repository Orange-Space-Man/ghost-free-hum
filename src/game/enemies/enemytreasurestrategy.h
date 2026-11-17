#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/events/missionresources.h"
#include "game/enemies/enemymanager.h"

#include <array>
#include <functional>
#include <vector>

namespace enemystrategy {
    class Treasure final : public Strategy {
    public:
        Treasure(const mission::Resources& mission, enemies::Runtime& runtime,
            const std::vector<std::uint32_t>& roomFlags,
            std::function<void(unsigned, const std::array<float, 3>&)> sound);
        void initializeBehavior() override;
        void behavior() override;

    private:
        const mission::Resources& mission;
        enemies::Runtime& runtime;
        const std::vector<std::uint32_t>& roomFlags;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::uint32_t room = 0;
        bool camera = false;
    };
}
