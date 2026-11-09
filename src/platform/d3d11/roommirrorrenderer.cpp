#include "platform/d3d11/roommirrorrenderer.h"

#include <d3dcompiler.h>
#include <stdexcept>
#include <string>

namespace {
    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }
    const char copySource[] = R"(
Texture2D<float4> source : register(t0);
float4 vertex(uint id : SV_VertexID) : SV_POSITION {
    return float4(id == 2 ? 3 : -1, id == 1 ? -3 : 1, 0, 1);
}
float4 pixel(float4 position : SV_POSITION) : SV_TARGET {
    int2 p = int2(position.xy) * 2 + int2(320 - MIRROR_WIDTH, 240 - MIRROR_HEIGHT);
    float3 color = (source.Load(int3(p, 0)).rgb + source.Load(int3(p + int2(1, 0), 0)).rgb +
        source.Load(int3(p + int2(0, 1), 0)).rgb + source.Load(int3(p + 1, 0)).rgb) * 0.25;
    uint3 bytes = uint3(saturate(color) * 255 + 0.5);
    uint3 packed = bytes >> uint3(3, 2, 3);
    bytes = (packed << uint3(3, 2, 3)) | (packed >> uint3(2, 4, 2));
    return float4(float3(bytes) / 255, 1);
}
)";
}

void roommirror::Renderer::load(ID3D11Device* device, const Owner& owner) {
    width = owner.width();
    height = owner.height();
    const auto widthText = std::to_string(width), heightText = std::to_string(height);
    const D3D_SHADER_MACRO definitions[]{{"MIRROR_WIDTH", widthText.c_str()}, {"MIRROR_HEIGHT", heightText.c_str()}, {nullptr, nullptr}};
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    check(D3DCompile(copySource, sizeof(copySource), nullptr, definitions, nullptr, "vertex", "vs_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()), "Cannot compile mirror copy vertex shader.");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, copyVertex.ReleaseAndGetAddressOf()),
        "Cannot create mirror copy vertex shader.");
    code.Reset(); errors.Reset();
    check(D3DCompile(copySource, sizeof(copySource), nullptr, definitions, nullptr, "pixel", "ps_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()), "Cannot compile mirror copy pixel shader.");
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, copyPixel.ReleaseAndGetAddressOf()),
        "Cannot create mirror copy pixel shader.");
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width; description.Height = height;
    description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
    check(device->CreateTexture2D(&description, nullptr, image.GetAddressOf()), "Cannot allocate mirror capture.");
    check(device->CreateRenderTargetView(image.Get(), nullptr, target.ReleaseAndGetAddressOf()), "Cannot create mirror target.");
    check(device->CreateShaderResourceView(image.Get(), nullptr, texture.ReleaseAndGetAddressOf()), "Cannot create mirror texture.");
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    check(device->CreateSamplerState(&sampling, sampler.ReleaseAndGetAddressOf()), "Cannot create mirror sampler.");
    modelrenderer::PipelineState state;
    modelrenderer::initializeModelState(state);
    state.cull = 0; state.depthEnabled = state.depthWrite = state.blendEnabled = false;
    copyState.load(device, state);
    state.cull = 2; state.depthEnabled = state.depthWrite = true;
    state.alpha = {7, 0, 0, 7, 0};
    modelmaterial::StageProgram program;
    modelmaterial::Stage stage;
    stage.coordinate = stage.texture = 0;
    stage.color = {15, 15, 15, 8};
    stage.alpha = {7, 7, 7, 6};
    program.coordinates.push_back({0, 1, 4, 60, -1});
    program.coordinateCount = program.stageCount = 1;
    program.stages.push_back(stage);
    material.load(device, program, {}, state);
    coordinates = modelrenderer::prepareCoordinates(program, {});
    geometry.load(device, {});
    const std::array<std::array<float, 3>, 4> positions{{{-float(width), float(height * 2), 0},
        {float(width), float(height * 2), 0}, {float(width), 0, 0}, {-float(width), 0, 0}}};
    const std::array<std::array<float, 2>, 4> uv{{{1, 0}, {0, 0}, {0, 1}, {1, 1}}};
    vertices.clear();
    for (unsigned corner : {0u, 1u, 2u, 0u, 2u, 3u}) {
        modeldraw::Vertex vertex{};
        for (unsigned row = 0; row < 3; ++row) vertex.position[row] =
            owner.matrix()[row * 4] * positions[corner][0] + owner.matrix()[row * 4 + 1] * positions[corner][1] + owner.matrix()[row * 4 + 3];
        vertex.color = {255, 255, 255, 255};
        vertex.coordinates[0] = uv[corner];
        vertices.push_back(vertex);
    }
}

void roommirror::Renderer::capture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* scene) {
    context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    const D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
    context->RSSetViewports(1, &viewport);
    copyState.bind(context);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(copyVertex.Get(), nullptr, 0);
    context->PSSetShader(copyPixel.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &scene);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

void roommirror::Renderer::draw(ID3D11DeviceContext* context, const actormodel::Matrix& view,
    const std::array<float, 16>& projection) {
    geometry.update(context, 0, vertices);
    context->PSSetShaderResources(0, 1, texture.GetAddressOf());
    context->PSSetSamplers(0, 1, sampler.GetAddressOf());
    material.draw(context, {}, geometry, 0, {view, view, projection}, coordinates, {});
}
