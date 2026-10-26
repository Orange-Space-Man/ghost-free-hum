#include "platform/d3d11/j3dcombinerrenderer.h"

#include <algorithm>
#include <stdexcept>

namespace {
    void checkOperation(const std::array<int, 5>& operation) {
        if (operation[0] < 0 || operation[0] > 1 || operation[1] != 0 || operation[2] != 0 ||
            operation[3] < 0 || operation[3] > 1 || operation[4] < 0 || operation[4] > 3)
            throw std::runtime_error("J3D combiner requires an unimplemented arithmetic mode.");
    }

    std::array<unsigned, 4> swap(const j3dmodel::TevState& state, int index) {
        std::array<unsigned, 4> result;
        const auto& channels = state.swaps.at(index);
        for (unsigned i = 0; i < result.size(); ++i) {
            if (channels[i] < 0 || channels[i] > 3) throw std::runtime_error("Invalid J3D combiner swap channel.");
            result[i] = channels[i];
        }
        return result;
    }
}

j3drenderer::CombinerProgram j3drenderer::buildCombinerProgram(const j3dmodel::MaterialState& state) {
    CombinerProgram result;
    const auto& combiner = state.combiner;
    if (!combiner.count || combiner.count > 16 || state.textureCoordinates > 8)
        throw std::runtime_error("J3D combiner requires initialized stage and coordinate counts.");
    result.program.stageCount = combiner.count;
    result.program.coordinateCount = state.textureCoordinates;
    for (unsigned i = 0; i < state.textureCoordinates; ++i) {
        const auto& coordinate = state.coordinates[i];
        result.program.coordinates.push_back({i, coordinate.info[0], coordinate.info[1], coordinate.info[2]});
    }
    for (unsigned i = 0; i < combiner.textures.size(); ++i)
        result.program.textures.push_back({i, combiner.textures[i]});
    for (unsigned i = 0; i < combiner.count; ++i) {
        const auto& input = combiner.stages[i];
        checkOperation(input.colorOperation);
        checkOperation(input.alphaOperation);
        if (std::any_of(input.indirect.begin(), input.indirect.end(), [](int value) { return value != 0; }))
            throw std::runtime_error("J3D combiner requires unimplemented indirect sampling.");
        modelmaterial::Stage stage;
        stage.index = i;
        stage.coordinate = input.order[0];
        stage.texture = input.order[1];
        stage.raster = input.order[2];
        std::copy(input.colorInputs.begin(), input.colorInputs.end(), stage.color.begin());
        std::copy(input.alphaInputs.begin(), input.alphaInputs.end(), stage.alpha.begin());
        stage.colorOperation = input.colorOperation[0];
        stage.colorClamp = input.colorOperation[3] != 0;
        stage.colorOutput = input.colorOperation[4];
        stage.alphaOperation = input.alphaOperation[0];
        stage.alphaClamp = input.alphaOperation[3] != 0;
        stage.alphaOutput = input.alphaOperation[4];
        result.program.stages.push_back(stage);
        result.registers[i] = {swap(combiner, input.swap[1]), swap(combiner, input.swap[0]),
            static_cast<unsigned>(input.colorSelection), static_cast<unsigned>(input.alphaSelection)};
    }
    return result;
}

void j3drenderer::CombinerShader::load(ID3D11Device* device, const j3dmodel::MaterialState& state,
    const modelrenderer::PipelineState& pipeline) {
    const auto program = buildCombinerProgram(state);
    shader.load(device, program.program, program.registers, pipeline);
}

void j3drenderer::CombinerShader::bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state) const {
    shader.bind(context, {state.combiner.colors, state.combiner.constants});
}
