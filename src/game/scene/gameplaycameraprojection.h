#pragma once

#include "game/actors/actorinitialpose.h"
#include "game/models/s3denvironment.h"
#include "game/scene/gameplaycameraowner.h"

namespace gameplaycamera {
    struct DrawState {
        s3denvironment::Camera camera;
        actormodel::Matrix view;
        std::array<float, 16> projection, worldProjection;
    };

    class Projection {
    public:
        explicit Projection(const std::vector<std::uint8_t>& executable);
        DrawState publish(CameraOwner& owner, const actormodel::InitialPose& pose) const;

    private:
        float shakeDecay, halfAngle, radians;
    };
}
