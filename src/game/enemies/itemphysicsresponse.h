#pragma once

#include "game/world/mapcollisionstate.h"

namespace itemphysics {
    std::array<float, 3> reflection(const mapcollision::State& collision, std::int16_t polygon,
        const std::array<float, 3>& velocity, float reflect, float friction);
}
