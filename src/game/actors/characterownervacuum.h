#pragma once

#include "game/actors/characterowner.h"
#include "game/player/playervacuumeffects.h"
#include "game/actors/actormodelcatalog.h"

#include <functional>

namespace characterowner {
    void updateVacuum(Owner& owner, playermodels::VacuumEntries& entries, modeleffects::Factory& effects,
        const enemytypes::Manager& parameters, const mapcollision::State& collision,
        const actormodel::Catalog& models, enemies::Runtime& runtime);
    void updateSuction(Owner& owner, playermodels::VacuumEntries& entries, const actors::Registry& actors,
        const enemycreation::Catalog& creation,
        const std::function<void(unsigned, const std::array<float, 3>&, unsigned)>& sound);
}
