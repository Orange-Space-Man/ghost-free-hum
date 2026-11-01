#include "playersheetrenderer.h"
#include "jlib/j3d/j3dshapevertices.h"

#include <cstring>
#include <limits>
#include <stdexcept>

playersheets::Renderer::Renderer(ID3D11Device* source) : device(source) {
    vertexShader.load(source);
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = 0;
    if (FAILED(device->CreateSamplerState(&sampling, sampler.GetAddressOf())))
        throw std::runtime_error("Cannot create direct cloth sampler.");
}

void playersheets::Renderer::bindTexture(ID3D11DeviceContext* context, unsigned slot, const playermodels::GridTexture& texture) {
    if (!context || slot >= 8 || !texture.primary)
        throw std::runtime_error("Cloth texture binding requires a retained resource and valid slot.");
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(owner.GetAddressOf());
    if (owner.Get() != device.Get()) throw std::runtime_error("Cloth texture context belongs to another device.");
    if (texture.wrapS || texture.wrapT || texture.minFilter != 1 || texture.magFilter != 1 ||
        texture.minLod || texture.maxLod || texture.lodBias || texture.mipmap)
        throw std::runtime_error("Cloth sampler differs from the recovered direct draw setup.");
    auto found = textures.find(texture.primary);
    if (found == textures.end()) {
        j3dmodel::Geometry source;
        source.bytes = texture.primary;
        source.textureHeaders = {0};
        j3drenderer::Textures uploaded;
        uploaded.load(device.Get(), source);
        found = textures.emplace(texture.primary, std::move(uploaded)).first;
    }
    auto* view = found->second.view(0);
    context->PSSetShaderResources(slot, 1, &view);
    context->PSSetSamplers(slot, 1, sampler.GetAddressOf());
}

void playersheets::Renderer::draw(ID3D11DeviceContext* context, const std::vector<std::array<DrawVertex, 4>>& quads,
    const j3dmodel::MaterialState& state, const actormodel::Matrix& camera, const std::array<float, 16>& projection) {
    if (!context) throw std::runtime_error("Cloth submission requires a D3D context.");
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(owner.GetAddressOf());
    if (owner.Get() != device.Get()) throw std::runtime_error("Cloth context belongs to another device.");
    if (quads.empty()) return;
    if (quads.size() > std::numeric_limits<unsigned>::max() / (6 * sizeof(j3dmodel::ShapeVertex)))
        throw std::runtime_error("Cloth vertex allocation exceeds D3D limits.");
    const auto& pixels = state.pixels;
    if (pixels.blend[0] == 2 || pixels.blend[0] > 3 || pixels.dither || (pixels.fog && (*pixels.fog)[0]))
        throw std::runtime_error("Cloth submission requires unsupported raster processing.");
    modelrenderer::PipelineState pipeline;
    pipeline.alpha = {pixels.alpha[0], pixels.alpha[1], pixels.alpha[2], pixels.alpha[3], pixels.alpha[4]};
    pipeline.depthEnabled = pixels.depth[0] != 0;
    pipeline.depthComparison = pixels.depth[1];
    pipeline.depthWrite = pixels.depth[2] != 0;
    pipeline.earlyDepth = pixels.depthLocation != 0;
    pipeline.cull = state.cullMode;
    pipeline.blendEnabled = pixels.blend[0] == 1;
    pipeline.subtract = pixels.blend[0] == 3;
    pipeline.sourceBlend = pixels.blend[1];
    pipeline.destinationBlend = pixels.blend[2];
    const auto key = j3drenderer::combinerKey(state);
    auto shader = shaders.find(key);
    if (shader == shaders.end()) {
        j3drenderer::CombinerShader created;
        created.load(device.Get(), state, pipeline);
        shader = shaders.emplace(key, std::move(created)).first;
    }
    j3dmodel::NormalMatrix normal;
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned column = 0; column < 3; ++column) normal[row * 3 + column] = camera[row * 4 + column];
    vertexShader.bindMatrices(context, state, camera, normal, projection);
    const unsigned count = static_cast<unsigned>(quads.size() * 6);
    if (count > capacity) {
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = count * sizeof(j3dmodel::ShapeVertex);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Microsoft::WRL::ComPtr<ID3D11Buffer> replacement;
        if (FAILED(device->CreateBuffer(&description, nullptr, replacement.GetAddressOf())))
            throw std::runtime_error("Cannot allocate cloth vertices.");
        vertices = std::move(replacement);
        capacity = count;
    }
    std::vector<j3dmodel::ShapeVertex> converted;
    converted.reserve(count);
    for (const auto& quad : quads) {
        for (unsigned index : {0U, 1U, 2U, 0U, 2U, 3U}) {
            j3dmodel::ShapeVertex vertex;
            vertex.position = quad[index].position;
            vertex.normals[0] = quad[index].normal;
            vertex.coordinates[0] = quad[index].texture;
            converted.push_back(vertex);
        }
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        throw std::runtime_error("Cannot update cloth vertices.");
    std::memcpy(mapped.pData, converted.data(), converted.size() * sizeof(j3dmodel::ShapeVertex));
    context->Unmap(vertices.Get(), 0);
    const unsigned stride = sizeof(j3dmodel::ShapeVertex), offset = 0;
    context->IASetVertexBuffers(0, 1, vertices.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    shader->second.bind(context, state);
    context->Draw(count, 0);
}
