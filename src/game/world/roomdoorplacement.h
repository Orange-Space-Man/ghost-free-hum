#pragma once

#include "game/world/roomobjectplacement.h"

namespace roomdoors {
    bool read(const std::vector<std::uint8_t>& executable, unsigned room,
        std::vector<roomobjects::Placement>& placements, std::string& error);
}
