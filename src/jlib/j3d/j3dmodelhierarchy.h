#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace j3dmodel {
    struct JointLinks {
        int child = -1, sibling = -1, material = -1;
    };

    struct MaterialLinks {
        int next = -1, shape = -1;
    };

    struct Hierarchy {
        int root = -1;
        std::vector<JointLinks> joints;
        std::vector<MaterialLinks> materials;
        std::vector<bool> boundShapes;
    };

    bool readHierarchy(const std::vector<std::uint8_t>& bytes, unsigned offset, unsigned jointCount,
        unsigned materialCount, unsigned shapeCount, Hierarchy& result, std::string& error);
}
