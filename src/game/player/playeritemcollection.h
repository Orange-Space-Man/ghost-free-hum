#pragma once

#include "game/actors/actorregistry.h"
#include "game/enemies/itemspawnadmission.h"

#include <functional>

namespace playerevents {
    struct ItemRequest {
        bool animated = true;
        std::int32_t actor = -1;
        std::int32_t gradeIndex = -1, grade = -1;
    };

    struct ItemAward {
        std::uint32_t kind, frames;
    };

    struct ItemEnvironment {
        const actors::Registry& actors;
        enemies::Manager& enemies;
        const characteritems::Admission& admission;
        ItemAward* award;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
    };

    bool acceptItem(playeractor::Player& player, const ItemRequest& request, const ItemEnvironment& environment);
}
