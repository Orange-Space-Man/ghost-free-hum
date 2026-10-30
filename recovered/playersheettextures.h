#pragma once

#include "playersheetlayout.h"

namespace playersheets {
    const playermodels::GridTexture& bindTextures(const Layout& layout, const playermodels::SharedModels& shared,
        playermodels::GridModels& grid);
}
