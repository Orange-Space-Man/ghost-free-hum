#include "platform/d3d11/modelbatchrenderer.h"
#include "game/models/modeldrawmatrices.h"

#include <stdexcept>
#include <utility>

void modelrenderer::Batches::load(ID3D11Device* device, const doors::ModelResource& resource) {
    Batches result;
    result.owner = device;
    result.geometry.load(device, resource);
    result.textures.load(device, resource);
    result.drawing = resource.drawing;
    result.materials = resource.materials;
    result.transforms = resource.textureTransforms;
    *this = std::move(result);
}

void modelrenderer::Batches::draw(ID3D11DeviceContext* context, const modeldraw::DrawCommand& command, unsigned pass,
    std::uint8_t mode, const VertexMatrices& matrices, const originalmath::FloatTrig& trig, modellighting::State& lighting,
    PipelineState& pipeline, const CombinerInputs& combiner, std::array<StageRegisters, 16>& retained, bool shadow) {
    if (!owner || !context) throw std::runtime_error("Model batches are not ready.");
    if (shadow) throw std::runtime_error("Projected model shadows require the original shadow texture-stage prefix.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("Model batch context belongs to another device.");
    const auto& batch = drawing.batch(command.batch);
    const auto& material = materials.at(command.material);
    auto lightState = lighting;
    lightState.material.lightingFlags = command.lightingFlags;
    auto rasterState = pipeline;
    bool specular = false;
    if (!command.alternateMaterial)
        specular = modellighting::configureMaterial(lightState, material, command.opacity, batch.lighting, command.lightingFlags);
    const auto program = command.alternateMaterial ? modelmaterial::buildStages({}, 0, 0, false, false) :
        modelmaterial::buildStages(material, batch.textureCount, batch.normalMode, specular, shadow);
    std::map<int, dooranimation::Matrix> textureMatrices;
    for (const auto& coordinate : program.coordinates) {
        if (coordinate.generatedMatrix >= 0 && !textureMatrices.count(coordinate.generatedMatrix))
            textureMatrices.emplace(coordinate.generatedMatrix, modelmaterial::evaluateTransform(transforms.at(coordinate.generatedMatrix),
                modeldraw::textureMatrix(matrices.modelView), trig));
    }
    std::optional<std::array<float, 3>> lightPosition;
    if (lightState.loadedMask & 1) {
        const auto& position = lightState.lights[0].position;
        lightPosition = {position[0], position[1], position[2]};
    }
    const auto coordinates = prepareCoordinates(program, textureMatrices, lightPosition);
    const auto inputs = modellighting::prepare(lightState);
    selectModelPass(rasterState, pass);
    selectBatchCull(rasterState, batch.cull, mode);
    const auto& alpha = rasterState.alpha;
    std::vector<unsigned> key{static_cast<unsigned>(command.material), static_cast<unsigned>(command.batch),
        unsigned(command.alternateMaterial), unsigned(specular), unsigned(shadow), alpha.first, alpha.firstReference,
        alpha.operation, alpha.second, alpha.secondReference, unsigned(rasterState.depthEnabled), unsigned(rasterState.depthWrite),
        unsigned(rasterState.earlyDepth), rasterState.depthComparison, rasterState.cull, unsigned(rasterState.blendEnabled),
        unsigned(rasterState.subtract), unsigned(rasterState.colorWrite), unsigned(rasterState.alphaWrite),
        unsigned(rasterState.targetHasAlpha), rasterState.sourceBlend, rasterState.destinationBlend, unsigned(rasterState.scissor)};
    for (const auto& stage : retained) {
        key.insert(key.end(), stage.textureSwap.begin(), stage.textureSwap.end());
        key.insert(key.end(), stage.rasterSwap.begin(), stage.rasterSwap.end());
        key.push_back(stage.colorConstant);
        key.push_back(stage.alphaConstant);
    }
    auto found = shaders.find(key);
    if (found == shaders.end()) {
        MaterialShader shader;
        shader.load(owner.Get(), program, retained, rasterState);
        found = shaders.emplace(std::move(key), std::move(shader)).first;
    }
    textures.bind(context, command.material, program);
    found->second.draw(context, combiner, geometry, command.batch, matrices, coordinates, inputs);
    lighting = lightState;
    pipeline = rasterState;
    for (const auto& stage : program.stages) {
        auto& registers = retained[stage.index];
        if (stage.resetSwap) registers.textureSwap = registers.rasterSwap = {0, 1, 2, 3};
        if (stage.setKonst) {
            registers.colorConstant = stage.konstColor;
            registers.alphaConstant = stage.konstAlpha;
        }
    }
}

std::size_t modelrenderer::Batches::shaderCount() const { return shaders.size(); }
