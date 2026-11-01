#include "playersheetshadow.h"

#include <stdexcept>

namespace {
    actormodel::Matrix concatenate(const actormodel::Matrix& first, const actormodel::Matrix& second) {
        actormodel::Matrix result;
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 4; ++column) {
                result[row * 4 + column] = first[row * 4] * second[column] +
                    first[row * 4 + 1] * second[4 + column] + first[row * 4 + 2] * second[8 + column];
                if (column == 3) result[row * 4 + column] += first[row * 4 + 3];
            }
        }
        return result;
    }
}

playersheets::DrawSlots playersheets::beginShadow(const ShadowState& shadow, j3dmodel::MaterialState& state,
    ShadowTextures& textures) {
    state.combiner.constants[3] = shadow.color;
    state.combiner.swaps[1] = {0, 3, 2, 3};
    state.combiner.swaps[2] = {3, 1, 2, 3};
    state.textureMatrix(118, shadow.postMatrices[0], 0);
    state.textureCoordinate(0, {0, 0, 33}, 0, 118);
    textures.bind(0);
    state.textureMatrix(121, shadow.postMatrices[1], 0);
    state.textureCoordinate(1, {0, 0, 33}, 0, 121);
    textures.bind(1);

    auto& first = state.combiner.stages[0];
    first.swap = {0, 1};
    first.colorInputs = {15, 15, 15, 8};
    first.colorOperation = {0, 0, 0, 1, 0};
    first.alphaInputs = {7, 7, 7, 7};
    first.alphaOperation = {0, 0, 0, 0, 0};
    first.order = {0, 0, 255};

    auto& second = state.combiner.stages[1];
    second.colorSelection = 31;
    second.alphaSelection = 31;
    second.swap = {0, 2};
    second.colorInputs = {0, 8, 15, 15};
    second.colorOperation = {10, 0, 0, 1, 0};
    second.alphaInputs = {7, 7, 6, 7};
    second.alphaOperation = {10, 0, 0, 1, 1};
    second.order = {1, 1, 255};
    state.combiner.stages[2].swap = {0, 0};
    state.combiner.stages[4].swap = {0, 0};
    return {2, 2, 2};
}

void playersheets::projectShadow(const ShadowState& shadow, const actormodel::Matrix& model,
    j3dmodel::MaterialState& state) {
    state.textureMatrix(30, concatenate(shadow.projections[0], model), 0);
    state.textureCoordinate(0, {0, 0, 30}, 0, 125);
    state.textureMatrix(33, concatenate(shadow.projections[1], model), 0);
    state.textureCoordinate(1, {0, 0, 33}, 0, 125);
}

unsigned playersheets::loadSceneLights(const std::vector<j3dmodel::MaterialLight>& lights,
    j3dmodel::MaterialState& state) {
    if (lights.size() > state.lights.size()) throw std::runtime_error("Original scene light slots exceeded.");
    unsigned mask = 0;
    for (unsigned i = 0; i < lights.size(); ++i) {
        state.light(i, lights[i]);
        mask += 1U << i;
    }
    return mask;
}
