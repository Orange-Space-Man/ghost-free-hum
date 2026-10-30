#pragma once

#include "playersheetmotion.h"

#include <functional>

namespace playersheets {
    void updateMaterial(Grid& grid, const Motion& motion, playermodels::GridModels& models);
    void updateModelTransform(const Grid& grid, playermodels::GridModels& models, const playermodels::SharedModels& shared,
        const std::function<std::uint32_t(const playerscanner::Vector&)>& roomColor);
}
