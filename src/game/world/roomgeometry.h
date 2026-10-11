#pragma once

#include "game/world/roommodelreader.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace roomgeometry {
    using Position = std::array<float, 3>;

    struct Triangle {
        std::array<Position, 3> positions{};
        std::uint16_t material = 0;
        std::array<std::array<float, 2>, 3> coordinates{};
    };

    struct Mesh {
        std::vector<Triangle> triangles;
        Position minimum{};
        Position maximum{};
        std::size_t batches = 0;
    };

    bool decode(const std::vector<std::uint8_t>& data, Mesh& mesh, std::string& error,
        const std::vector<roommodel::Node>& pose = {});
}
