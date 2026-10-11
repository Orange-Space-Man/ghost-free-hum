#pragma once

#include "game/world/roomgeometry.h"

namespace roomobjects {
    struct Placement {
        std::string model;
        roomgeometry::Position position{};
        roomgeometry::Position rotation{};
        roomgeometry::Position scale{1, 1, 1};
    };

    bool read(const std::vector<std::uint8_t>& data, unsigned room, std::vector<Placement>& placements, std::string& error);
    roomgeometry::Position transform(const roomgeometry::Position& position, const Placement& placement);
}
