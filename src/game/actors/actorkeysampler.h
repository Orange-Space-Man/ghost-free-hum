#pragma once

#include "game/actors/actormodelinstance.h"

namespace actoranimation {
    struct JointPose {
        std::array<float, 3> scale;
        std::array<std::int32_t, 3> rotation;
        std::array<float, 3> translation;
    };

    JointPose sampleJoint(const actormodel::Resource& key, unsigned joint, double frame,
        actormodel::Attachment& cursors);
}
