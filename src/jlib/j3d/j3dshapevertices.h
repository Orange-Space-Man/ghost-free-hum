#pragma once

#include "jlib/j3d/j3dmodelgeometry.h"

namespace j3dmodel {
    struct ShapeVertex {
        std::array<float, 3> position{};
        std::array<std::array<float, 3>, 3> normals{};
        std::array<std::array<float, 2>, 8> coordinates{};
        std::array<std::uint8_t, 9> matrixIndices{};
        bool skipped = false;
    };

    struct ShapePrimitive {
        std::uint8_t command = 0;
        std::vector<ShapeVertex> vertices;
    };

    std::vector<ShapePrimitive> decodeShapeGroup(const Geometry& geometry, const Shape& shape,
        const ShapeGroup& group, bool nbtEnabled);
}
