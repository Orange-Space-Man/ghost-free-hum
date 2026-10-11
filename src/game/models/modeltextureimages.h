#pragma once

#include "game/models/modelmaterialstate.h"
#include "game/world/roommaterials.h"

namespace modelmaterial {
    struct TextureImages {
        std::vector<roommaterials::Texture> levels;
        std::vector<std::size_t> offsets;
        std::size_t encodedSize = 0;
    };

    bool readImages(const std::vector<std::uint8_t>& model, const Sampler& sampler,
        TextureImages& images, std::string& error);
}
