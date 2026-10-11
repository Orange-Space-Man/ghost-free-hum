#pragma once

#include "game/player/playerorientation.h"

namespace actors {
    struct RotatingSurface {
        std::array<float, 3> position{};
        float radius = 0;
        std::int16_t rotation = 0;
    };

    class RotatingSurfaces {
    public:
        bool velocity(const actormodel::InitialPose& pose, const std::array<float, 3>& position,
            std::array<float, 3>& result, playerorientation::Quaternion& orientation) const;
        std::array<RotatingSurface, 2> entries;
    };
}
