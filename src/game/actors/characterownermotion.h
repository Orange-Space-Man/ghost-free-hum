#pragma once

#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

namespace characterowner {
    void updateMotion(Owner& owner, const actormodel::Catalog& models, const actormodel::Resources& resources,
        const actormodel::InitialPose& pose, const std::array<float, 3>* originalScratch = nullptr);
}
