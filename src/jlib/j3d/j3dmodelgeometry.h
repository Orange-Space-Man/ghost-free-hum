#pragma once

#include "jlib/j3d/j3dmodelhierarchy.h"
#include "jlib/j3d/j3dnametable.h"

#include <array>
#include <memory>

namespace j3dmodel {
    struct Joint {
        unsigned index = 0;
        std::uint8_t kind = 0, scaleCompensation = 0;
        std::array<float, 3> scale{}, translation{}, minimum{}, maximum{};
        std::array<std::int16_t, 3> rotation{};
        float radius = 0;
    };

    struct ShapeGroup {
        unsigned matrixKind = 0;
        std::uint16_t matrix = 0;
        std::vector<std::uint16_t> matrixTable;
        unsigned drawOffset = 0, drawSize = 0;
    };

    struct Shape {
        unsigned descriptorOffset = 0;
        float radius = 0;
        std::array<float, 3> minimum{}, maximum{};
        std::vector<ShapeGroup> groups;
    };

    struct Envelope {
        std::vector<std::uint16_t> joints;
        std::vector<float> weights;
    };

    struct DrawMatrix {
        std::uint8_t weighted = 0;
        std::uint16_t index = 0;
    };

    struct Geometry {
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        std::uint32_t flags = 0, packetCount = 0, vertexCount = 0, normalCount = 0;
        unsigned vertexFormatOffset = 0;
        std::array<unsigned, 13> vertexOffsets{};
        unsigned inverseMatrixOffset = 0, materialOffset = 0, textureOffset = 0;
        std::vector<Joint> joints;
        std::vector<std::unique_ptr<Shape>> shapes;
        std::vector<Envelope> envelopes;
        std::vector<DrawMatrix> drawMatrices;
        std::unique_ptr<NameTable> jointNames, materialNames, shapeNames, textureNames;
        std::vector<unsigned> textureHeaders;
        Hierarchy hierarchy;
    };

    bool readGeometry(const std::vector<std::uint8_t>& bytes, std::uint32_t flags, Geometry& geometry, std::string& error);
}
