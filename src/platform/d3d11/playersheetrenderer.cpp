#include "platform/d3d11/playersheetrenderer.h"
#include "platform/d3d11/gameshadowrenderer.h"

#include <stdexcept>

void playersheets::Renderer::load(ID3D11Device* owner) {
    device = owner;
    textures.clear();
    geometry.load(device.Get(), {});
    modelrenderer::PipelineState state;
    modelrenderer::initializeModelState(state);
    state.cull = 0;
    state.depthComparison = 1;
    for (unsigned textured = 0; textured < 2; ++textured) {
        modelmaterial::StageProgram program;
        modelmaterial::Stage stage;
        stage.raster = 4;
        if (textured) {
            stage.coordinate = stage.texture = 0;
            stage.color = {15, 10, 8, 15};
            stage.alpha = {7, 5, 4, 7};
            program.coordinateCount = 1;
            program.coordinates.push_back({0, 1, 4, 60, -1});
        } else {
            stage.color = {15, 15, 15, 10};
            stage.alpha = {7, 7, 7, 5};
        }
        program.stages.push_back(stage);
        program.stageCount = 1;
        const auto shadow = modelmaterial::buildStages({}, 0, 0, false, true);
        for (unsigned i = 1; i < shadow.stages.size(); ++i) {
            auto value = shadow.stages[i];
            value.index = program.stageCount++;
            program.stages.push_back(value);
        }
        materials[textured].load(device.Get(), program, {}, state, true);
        coordinates[textured] = modelrenderer::prepareCoordinates(program, {});
    }
    D3D11_SAMPLER_DESC description{};
    description.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    description.AddressU = description.AddressV = description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(device->CreateSamplerState(&description, sampler.ReleaseAndGetAddressOf())))
        throw std::runtime_error("Cannot create original cloth sampler.");
}

ID3D11ShaderResourceView* playersheets::Renderer::texture(const playermodels::GridTexture& source) {
    const auto found = textures.find(source.primary);
    if (found != textures.end()) return found->second.Get();
    if (!source.primary || source.imageOffset > source.primary->size()) throw std::runtime_error("Cloth texture is unavailable.");
    roommaterials::Texture decoded;
    std::string error;
    if (!roommaterials::decodeTexture({source.primary->begin() + source.imageOffset, source.primary->end()},
        source.width, source.height, source.format, decoded, error)) throw std::runtime_error(error);
    D3D11_TEXTURE2D_DESC description{};
    description.Width = source.width;
    description.Height = source.height;
    description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{decoded.pixels.data(), source.width * 4, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(device->CreateTexture2D(&description, &data, image.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(image.Get(), nullptr, view.GetAddressOf())))
        throw std::runtime_error("Cannot upload original cloth texture.");
    return textures.emplace(source.primary, std::move(view)).first->second.Get();
}

void playersheets::Renderer::draw(ID3D11DeviceContext* context, const playermodels::GridModels& grid, unsigned pass,
    const modelrenderer::VertexMatrices& matrices, const modellighting::State& lighting) {
    if (!draws(grid, pass)) return;
    if (!grid.parameters.field7D || grid.parameters.field7C)
        throw std::runtime_error("Cloth drawing requires the original projected-light or reversed-opacity path.");
    const auto vertices = prepareGeometry(grid);
    if (vertices.empty()) return;
    geometry.update(context, 0, vertices);
    const unsigned textured = bool(grid.texture.primary);
    if (textured) {
        auto* view = texture(grid.texture);
        context->PSSetShaderResources(0, 1, &view);
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
    }
    auto state = lighting;
    state.material.color = {255, 255, 255, 255};
    for (unsigned channel : {0u, 2u}) state.material.channels[channel] = {1, 0, 0, lighting.material.lightMask, 2, 1};
    state.material.secondaryColor = {255, 255, 255, 255};
    state.material.channels[3] = {1, 0, 0, 0x80, 2, 1};
    modelrenderer::CombinerInputs inputs;
    inputs.constants[3] = gameshadowrenderer::color();
    materials[textured].draw(context, inputs, geometry, 0, matrices, coordinates[textured], modellighting::prepare(state));
}
