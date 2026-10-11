#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/scene/gameparameters.h"
#include "game/enemies/itemphysicsresponse.h"
#include "game/world/rotatingsurfacevelocity.h"

namespace enemystrategy {
    class ItemPhysics final : public Strategy {
    public:
        ItemPhysics(const mapcollision::State& collision, const actors::RotatingSurfaces& surfaces,
            const actormodel::InitialPose& pose);
        void initializeBehavior() override;
        void behavior() override;
        gameparameters::Parameters parameters;

    private:
        const mapcollision::State& collision;
        const actors::RotatingSurfaces& surfaces;
        const actormodel::InitialPose& pose;
        std::array<float, 3> acceleration;
    };
}
