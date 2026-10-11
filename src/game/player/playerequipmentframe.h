#pragma once

#include "game/player/playeractor.h"
#include <functional>

namespace playeractor {
    class EquipmentFrame {
    public:
        explicit EquipmentFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const std::function<void(unsigned, const std::array<float, 3>&)>& sound) const;

    private:
        float transitionRate;
        std::int32_t elementalDelay;
    };
}
