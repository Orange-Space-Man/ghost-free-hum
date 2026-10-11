#pragma once

#include "game/world/roomgeometry.h"

namespace roommaterials {
    struct Texture {
        unsigned width = 0;
        unsigned height = 0;
        std::vector<std::uint8_t> pixels;
    };

    struct Material {
        std::size_t texture = 0;
        unsigned wrapS = 0;
        unsigned wrapT = 0;
    };

    struct Collection {
        std::vector<Texture> textures;
        std::vector<Material> materials;
    };

    bool decodeTexture(const std::vector<std::uint8_t>& data, unsigned width, unsigned height,
        unsigned format, Texture& texture, std::string& error);
    bool read(const std::vector<std::uint8_t>& data, const roomgeometry::Mesh& mesh, Collection& collection, std::string& error);
}
