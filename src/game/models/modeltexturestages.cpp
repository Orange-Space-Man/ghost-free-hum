#include "game/models/modeltexturestages.h"

#include <stdexcept>

modelmaterial::StageProgram modelmaterial::buildStages(const Material& material, unsigned textureCount,
    std::uint8_t normalMode, bool specular, bool shadow, unsigned firstStage, unsigned firstCoordinate) {
    if (firstStage > 16 || firstCoordinate > 8) throw std::runtime_error("Invalid initial model texture-stage counters.");
    const auto plan = texturePlan(material, textureCount, normalMode);
    StageProgram result;
    result.stageCount = firstStage;
    result.coordinateCount = firstCoordinate;
    const auto stage = [&](Stage value) {
        if (result.stageCount >= 16) throw std::runtime_error("Model exceeds original texture-stage capacity.");
        value.index = result.stageCount++;
        result.stages.push_back(value);
    };
    const auto bind = [&](unsigned slot, bool generated) {
        const auto& sampler = material.samplers.at(slot);
        if (!sampler.present) throw std::runtime_error("Texture stage references an absent sampler.");
        if (result.coordinateCount >= 8) throw std::runtime_error("Model exceeds original texture-coordinate capacity.");
        const unsigned coordinate = result.coordinateCount++;
        result.textures.push_back({slot, coordinate});
        Coordinate value{coordinate, 1, slot + 4, 60, -1};
        if (generated && sampler.generatedMatrix >= 0) {
            value.source = 1;
            value.matrix = 30 + coordinate * 3;
            value.generatedMatrix = sampler.generatedMatrix;
        }
        result.coordinates.push_back(value);
        return coordinate;
    };
    if (plan.untextured) {
        Stage value;
        value.raster = 4;
        value.color = {15, 15, 15, 10};
        value.alpha = {7, 7, 7, 5};
        value.resetSwap = true;
        stage(value);
    }
    for (const auto& step : plan.steps) {
        Stage value;
        value.coordinate = value.texture = bind(step.slot, true);
        value.raster = 4;
        value.color = step.combine == 0 ? std::array<unsigned, 4>{15, 10, 8, 15} : std::array<unsigned, 4>{15, 8, 12, 0};
        value.alpha = {7, 5, 4, 7};
        value.resetSwap = true;
        stage(value);
    }
    if (plan.appendNormalMapping) {
        Stage first;
        first.coordinate = first.texture = bind(plan.normalSlot, false);
        first.raster = 4;
        first.color = {15, 8, 12, 10};
        first.alpha = {7, 7, 7, 6};
        first.colorClamp = first.alphaClamp = false;
        first.colorOutput = first.alphaOutput = 2;
        first.resetSwap = true;
        stage(first);
        if (result.coordinateCount >= 8) throw std::runtime_error("Model normal mapping exceeds coordinate capacity.");
        const unsigned coordinate = result.coordinateCount++;
        result.coordinates.push_back({coordinate, 2, 12 + coordinate - 1, 60, -1});
        Stage second;
        second.coordinate = coordinate;
        second.texture = first.texture;
        second.color = {15, 8, 12, 4};
        second.alpha = {7, 7, 7, 2};
        second.colorOperation = 1;
        second.colorOutput = second.alphaOutput = 2;
        second.alphaClamp = false;
        stage(second);
        Stage third;
        third.color = {15, 4, 0, 15};
        third.alpha = {7, 7, 7, 0};
        stage(third);
    }
    if (specular) {
        Stage value;
        value.raster = 5;
        value.color = {10, 15, 15, 0};
        value.alpha = {7, 7, 7, 0};
        stage(value);
    }
    if (shadow) {
        Stage first;
        first.raster = 5;
        first.color = {15, 15, 15, 0};
        first.alpha = {7, 5, 1, 7};
        first.colorOutput = first.alphaOutput = 2;
        stage(first);
        Stage second;
        second.color = {0, 14, 5, 15};
        second.alpha = {7, 7, 7, 0};
        second.setKonst = true;
        second.konstColor = 15;
        second.konstAlpha = 28;
        stage(second);
    }
    return result;
}
