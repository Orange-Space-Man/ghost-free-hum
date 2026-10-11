#pragma once

#include "game/world/roommaterials.h"

#include <filesystem>

namespace roomscene {
    struct Scene {
        roomgeometry::Mesh mesh;
        roommaterials::Collection materials;
        std::size_t objects = 0;
        std::size_t doors = 0;
    };

    bool read(const std::filesystem::path& disc, Scene& scene, std::string& error, unsigned room = 0);
}
