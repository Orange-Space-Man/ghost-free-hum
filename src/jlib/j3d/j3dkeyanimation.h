#pragma once

#include "jlib/j3d/j3danimationresource.h"

namespace j3dmodel {
    float sampleAnimationKey(const Animation& animation, const AnimationGroup& group,
        unsigned descriptor, unsigned channel, bool shortValues, float fallback);
}
