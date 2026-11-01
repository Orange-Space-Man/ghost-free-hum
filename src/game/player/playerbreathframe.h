#pragma once

#include "game/player/playeractor.h"

namespace playeractor {
    class BreathFrame {
    public:
        explicit BreathFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const actormodel::Catalog& catalog, const actormodel::Resources& resources,
            bool roomCleared) const;

    private:
        std::array<float, 4> parameters;
    };
}
