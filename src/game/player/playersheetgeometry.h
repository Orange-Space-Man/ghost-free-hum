#pragma once

#include "game/player/playergridmodels.h"
#include "game/models/modelvertexstream.h"

namespace playersheets {
    std::vector<modeldraw::Vertex> prepareGeometry(const playermodels::GridModels& grid);
    bool draws(const playermodels::GridModels& grid, unsigned pass);
}
