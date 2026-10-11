#pragma once

#include "game/models/modelmaterialstate.h"

namespace modelmaterial {
    struct Stage {
        unsigned index = 0, coordinate = 255, texture = 255, raster = 255;
        std::array<unsigned, 4> color{}, alpha{};
        unsigned colorOperation = 0, alphaOperation = 0, colorOutput = 0, alphaOutput = 0;
        bool colorClamp = true, alphaClamp = true, resetSwap = false, setKonst = false;
        unsigned konstColor = 0, konstAlpha = 0;
    };

    struct Coordinate {
        unsigned index = 0, type = 1, source = 0, matrix = 60;
        int generatedMatrix = -1;
    };

    struct TextureBinding {
        unsigned slot = 0, texture = 0;
    };

    struct StageProgram {
        std::vector<Stage> stages;
        std::vector<Coordinate> coordinates;
        std::vector<TextureBinding> textures;
        unsigned stageCount = 0, coordinateCount = 0;
    };

    StageProgram buildStages(const Material& material, unsigned textureCount, std::uint8_t normalMode,
        bool specular, bool shadow, unsigned firstStage = 0, unsigned firstCoordinate = 0);
}
