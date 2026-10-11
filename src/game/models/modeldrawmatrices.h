#pragma once

#include "game/world/dooranimationpose.h"

namespace modeldraw {
    bool normalMatrix(const dooranimation::Matrix& modelView, dooranimation::Matrix& output);
    dooranimation::Matrix textureMatrix(const dooranimation::Matrix& modelView);
}
