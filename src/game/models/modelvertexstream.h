#pragma once

#include "game/models/modeldrawresource.h"

namespace modeldraw {
    struct Vertex {
        std::array<float, 3> position{};
        std::array<std::array<float, 3>, 3> normals{};
        std::array<std::uint8_t, 4> color{};
        std::array<std::array<float, 2>, 4> coordinates{};
    };

    struct Primitive {
        std::uint8_t command = 0;
        std::vector<Vertex> vertices;
    };

    bool decodeVertices(const std::vector<std::uint8_t>& model, const Batch& batch,
        std::vector<Primitive>& primitives, std::string& error);
}
