#include "platform/d3d11/particlebillboardrenderer.h"

#include <d3dcompiler.h>
#include <cstring>
#include <stdexcept>

namespace {
    struct Vertex {
        std::array<float, 3> position;
        std::array<float, 2> uv;
    };

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    const char vertexSource[] = R"(
cbuffer Matrices : register(b0) { float4 view[3]; float4 projection[4]; };
struct Input { float3 position : POSITION; float2 uv : TEXCOORD0; };
struct Output {
    float4 position : SV_POSITION; float4 color : COLOR0;
    float3 normal : NORMAL0; float3 binormal : NORMAL1; float3 tangent : NORMAL2;
    float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float2 uv2 : TEXCOORD2; float2 uv3 : TEXCOORD3;
    float3 viewPosition : TEXCOORD4; float2 generated[8] : TEXCOORD5;
    float4 secondaryColor : COLOR1; float4 generatedQ[2] : TEXCOORD13;
};
Output main(Input input) {
    Output output = (Output)0;
    float4 world = float4(input.position, 1);
    float4 cameraPosition = float4(dot(view[0], world), dot(view[1], world), dot(view[2], world), 1);
    output.position = float4(dot(projection[0], cameraPosition), dot(projection[1], cameraPosition),
        dot(projection[2], cameraPosition), dot(projection[3], cameraPosition));
    output.generated[0] = input.uv;
    output.generatedQ[0] = output.generatedQ[1] = 1;
    return output;
}
)";
}

void particlerenderer::Renderer::load(ID3D11Device* owner) {
    device = owner;
    textures.clear();
    materials.clear();
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    const auto compiled = D3DCompile(vertexSource, sizeof(vertexSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, code.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(compiled)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Particle vertex compilation failed.");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vertexShader.ReleaseAndGetAddressOf()),
        "Cannot create particle vertex shader.");
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    check(device->CreateInputLayout(elements, 2, code->GetBufferPointer(), code->GetBufferSize(), layout.ReleaseAndGetAddressOf()),
        "Cannot create particle vertex layout.");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(Vertex) * 6;
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    check(device->CreateBuffer(&buffer, nullptr, vertices.ReleaseAndGetAddressOf()), "Cannot create particle vertex buffer.");
    buffer.ByteWidth = sizeof(float) * 28;
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer.CPUAccessFlags = 0;
    check(device->CreateBuffer(&buffer, nullptr, matrices.ReleaseAndGetAddressOf()), "Cannot create particle matrix buffer.");
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = 0;
    constexpr D3D11_TEXTURE_ADDRESS_MODE modes[] = {
        D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR};
    for (unsigned u = 0; u < 3; ++u) for (unsigned v = 0; v < 3; ++v) {
        sampling.AddressU = modes[u];
        sampling.AddressV = modes[v];
        check(device->CreateSamplerState(&sampling, samplers[u * 3 + v].ReleaseAndGetAddressOf()), "Cannot create particle sampler.");
    }
}

particlerenderer::Renderer::Texture& particlerenderer::Renderer::texture(const particles::Resource& source) {
    const auto found = textures.find(source);
    if (found != textures.end()) return found->second;
    if (!source || source->size() < 32) throw std::runtime_error("Particle texture header is truncated.");
    roommaterials::Texture decoded;
    std::string error;
    const unsigned width = (unsigned(source->at(2)) << 8) | source->at(3);
    const unsigned height = (unsigned(source->at(4)) << 8) | source->at(5);
    if (!roommaterials::decodeTexture({source->begin() + 32, source->end()}, width, height, source->at(0), decoded, error))
        throw std::runtime_error(error);
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{decoded.pixels.data(), width * 4, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
    check(device->CreateTexture2D(&description, &data, image.GetAddressOf()), "Cannot upload particle texture.");
    Texture result;
    check(device->CreateShaderResourceView(image.Get(), nullptr, result.view.GetAddressOf()), "Cannot create particle texture view.");
    return textures.emplace(source, std::move(result)).first->second;
}

void particlerenderer::Renderer::prepare(const particles::EmitterPool& pool) {
    for (const auto& list : pool.active) {
        for (auto* emitter = list.sentinel.next; emitter != &list.sentinel; emitter = emitter->next) {
            if (!emitter->draw || emitter->draw != &emitter->ordinaryDraw) continue;
            particles::Billboard item;
            item.texture = emitter->draw->texture;
            item.mode = emitter->draw->fields18[0];
            item.blend = emitter->parameters.fields206[1];
            item.depth = emitter->parameters.fields206[2];
            texture(item.texture);
            material(item);
        }
    }
    particles::Billboard glow;
    glow.mode = 4;
    glow.blend = 0x14;
    glow.depth = 0x0b;
    glow.cull = 0;
    material(glow);
}

void particlerenderer::Renderer::prepare(const particles::Billboard& billboard) {
    texture(billboard.texture);
    material(billboard);
}

modelrenderer::MaterialShader& particlerenderer::Renderer::material(const particles::Billboard& particle) {
    const unsigned key = particle.mode | (particle.blend << 8) | (particle.depth << 16) | (particle.cull << 24);
    const auto found = materials.find(key);
    if (found != materials.end()) return found->second;
    static const std::array<unsigned, 4> colors[] = {{2, 12, 8, 15}, {15, 2, 8, 15},
        {4, 2, 8, 15}, {15, 8, 2, 4}, {15, 8, 15, 2}, {15, 4, 8, 15}};
    if (particle.mode > 5) throw std::runtime_error("Particle color combiner mode is invalid.");
    modelmaterial::Stage stage;
    stage.coordinate = stage.texture = 0;
    stage.color = colors[particle.mode];
    stage.alpha = particle.mode == 5 ? std::array<unsigned, 4>{1, 7, 7, 4} : std::array<unsigned, 4>{7, 4, 1, 7};
    stage.alphaOperation = particle.mode == 5 ? 1 : 0;
    modelmaterial::StageProgram program;
    program.stageCount = program.coordinateCount = 1;
    program.stages.push_back(stage);
    program.coordinates.push_back({0, 1, 4, 60, -1});
    modelrenderer::PipelineState state;
    modelrenderer::initializeModelState(state);
    state.cull = particle.cull;
    state.sourceBlend = particle.blend & 15;
    state.destinationBlend = particle.blend >> 4;
    state.depthEnabled = (particle.depth & 8) != 0;
    state.depthComparison = particle.depth & 7;
    state.depthWrite = (particle.depth >> 4) != 0;
    modelrenderer::MaterialShader shader;
    shader.load(device.Get(), program, {}, state);
    return materials.emplace(key, std::move(shader)).first->second;
}

void particlerenderer::Renderer::draw(ID3D11DeviceContext* context, const std::vector<particles::Billboard>& particles,
    const std::array<float, 12>& view, const std::array<float, 16>& projection) {
    if (particles.empty()) return;
    if (!device) throw std::logic_error("Particle renderer has no device.");
    std::array<float, 28> transforms;
    std::copy(view.begin(), view.end(), transforms.begin());
    std::copy(projection.begin(), projection.end(), transforms.begin() + 12);
    context->UpdateSubresource(matrices.Get(), 0, nullptr, transforms.data(), 0, 0);
    context->VSSetConstantBuffers(0, 1, matrices.GetAddressOf());
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->IASetInputLayout(layout.Get());
    const unsigned stride = sizeof(Vertex), offset = 0;
    context->IASetVertexBuffers(0, 1, vertices.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    constexpr unsigned corners[] = {0, 1, 2, 0, 2, 3};
    for (const auto& particle : particles) {
        if (particle.textureWrap[0] > 2 || particle.textureWrap[1] > 2)
            throw std::runtime_error("Invalid particle texture wrapping.");
        context->PSSetSamplers(0, 1, samplers[particle.textureWrap[0] * 3 + particle.textureWrap[1]].GetAddressOf());
        std::array<Vertex, 6> output;
        for (unsigned i = 0; i < 6; ++i) output[i] = {particle.positions[corners[i]],
            {particle.coordinates[corners[i]][0] * particle.textureScale, particle.coordinates[corners[i]][1] * particle.textureScale}};
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Cannot update particle vertices.");
        std::memcpy(mapped.pData, output.data(), sizeof(output));
        context->Unmap(vertices.Get(), 0);
        const auto& image = texture(particle.texture);
        context->PSSetShaderResources(0, 1, image.view.GetAddressOf());
        modelrenderer::CombinerInputs colors;
        for (unsigned i = 0; i < 4; ++i) {
            colors.registers[1][i] = particle.color[i];
            colors.registers[2][i] = particle.secondary[i];
        }
        material(particle).bind(context, colors);
        context->Draw(6, 0);
    }
}
