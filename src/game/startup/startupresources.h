#pragma once

#include "game/world/roommaterials.h"
#include "jlib/jaudio/jaudioresources.h"
#include <filesystem>

namespace startupresources {
    struct Pane {
        std::string tag;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool visible = true;
        unsigned alpha = 255;
        std::size_t texture = 0;
    };

    struct Resources {
        std::vector<roommaterials::Texture> textures;
        std::vector<Pane> panes;
        std::vector<std::int16_t> logoSamples;
        std::vector<float> sine;
        std::vector<std::uint8_t> initialSaveFlags;
        jaudio::Resources audio;
    };

    bool read(const std::filesystem::path& package, Resources& resources, std::string& error);
    bool decodeTexture(const std::vector<std::uint8_t>& data, roommaterials::Texture& texture, std::string& error);
    bool readLayout(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& archive,
        Resources& resources, std::string& error);
}
