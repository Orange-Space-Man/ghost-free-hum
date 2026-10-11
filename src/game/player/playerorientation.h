#pragma once

#include "game/actors/actorinitialpose.h"

namespace playerorientation {
    using Quaternion = std::array<float, 4>;
    Quaternion fromMatrix(const actormodel::Matrix& matrix);
    actormodel::Matrix toMatrix(const Quaternion& quaternion);
    actormodel::Matrix blend(double weight, const actormodel::Matrix& first, const actormodel::Matrix& second);
    std::array<float, 3> direction(const Quaternion& quaternion);
}
