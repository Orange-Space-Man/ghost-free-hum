#pragma once

#include "game/actors/actorcapturemotion.h"
#include "game/actors/actorregistry.h"
#include "game/player/playervacuumeffects.h"
#include <functional>

namespace actorcapture {
    struct PoseBinding {
        const playermodels::VacuumPath* path = nullptr;
        const Vector* direction = nullptr;
    };

    PoseBinding resolvePoseBinding(const playermodels::VacuumEntries& entries, const actors::MoveActor& actor,
        const std::function<const Vector*()>& vacuumDirection);
    void updateCapturedActors(actors::Registry& registry, Motion& motion, const playermodels::VacuumEntries& entries,
        const std::function<const Vector*()>& vacuumDirection, actormodel::Matrix& retainedTransform);
}
