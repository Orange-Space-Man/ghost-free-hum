#pragma once

#include "game/actors/charactercreationstate.h"

#include <functional>

namespace characterowner {
    void updateDamage(Owner& owner, const charactercreation::Properties& properties, const actormodel::Catalog& models,
        playerrecord::Record& player, const std::function<void(unsigned, const std::array<float, 3>&)>& effect);
}
