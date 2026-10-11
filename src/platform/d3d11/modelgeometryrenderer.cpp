#include "platform/d3d11/modelgeometryrenderer.h"

#include <d3dcompiler.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    using Microsoft::WRL::ComPtr;

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    const char vertexSource[] = R"(
cbuffer Matrices : register(b0) {
    float4 modelView[3];
    float4 normalMatrix[3];
    float4 projection[4];
};
struct Coordinate { uint4 source; float4 transform[3]; };
cbuffer TextureCoordinates : register(b2) {
    Coordinate coordinates[8];
    float4 lightPosition;
};
struct Input {
    float3 position : POSITION;
    float3 normal : NORMAL0;
    float3 binormal : NORMAL1;
    float3 tangent : NORMAL2;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
};
struct Output {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float3 binormal : NORMAL1;
    float3 tangent : NORMAL2;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float3 viewPosition : TEXCOORD4;
    float2 generated[8] : TEXCOORD5;
    float4 secondaryColor : COLOR1;
};
float3 transformNormal(float3 value) {
    return float3(dot(normalMatrix[0].xyz, value), dot(normalMatrix[1].xyz, value), dot(normalMatrix[2].xyz, value));
}
Output main(Input input) {
    Output output = (Output)0;
    float4 position = float4(input.position, 1);
    float4 view = float4(dot(modelView[0], position), dot(modelView[1], position), dot(modelView[2], position), 1);
    output.position = float4(dot(projection[0], view), dot(projection[1], view), dot(projection[2], view), dot(projection[3], view));
    output.viewPosition = view.xyz;
    output.normal = transformNormal(input.normal);
    output.binormal = transformNormal(input.binormal);
    output.tangent = transformNormal(input.tangent);
    float3 lightingNormal = normalize(output.normal);
    int4 vertexColor = int4(round(input.color * 255));
    output.color = float4(illuminate(0, view.xyz, lightingNormal, vertexColor).rgb,
        illuminate(2, view.xyz, lightingNormal, vertexColor).a) / 255.0;
    output.secondaryColor = float4(illuminate(1, view.xyz, lightingNormal, vertexColor).rgb,
        illuminate(3, view.xyz, lightingNormal, vertexColor).a) / 255.0;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.uv2 = input.uv2;
    output.uv3 = input.uv3;
    [unroll]
    for (uint i = 0; i < 8; ++i) {
        output.generated[i] = 0;
        if (coordinates[i].source.x == 1) {
            uint source = coordinates[i].source.y;
            float4 value;
            if (source == 1) value = float4(input.normal, 1);
            else value = float4(source == 4 ? input.uv0 : source == 5 ? input.uv1 : source == 6 ? input.uv2 : input.uv3, 1, 1);
            output.generated[i] = float2(dot(coordinates[i].transform[0], value), dot(coordinates[i].transform[1], value));
        } else if (coordinates[i].source.x == 2) {
            float3 direction = normalize(lightPosition.xyz - view.xyz);
            output.generated[i] = output.generated[coordinates[i].source.y] +
                float2(dot(direction, output.tangent), dot(direction, output.binormal));
        }
    }
    return output;
}

)";
}

modelrenderer::TextureCoordinates modelrenderer::prepareCoordinates(const modelmaterial::StageProgram& program,
    const std::map<int, dooranimation::Matrix>& matrices, const std::optional<std::array<float, 3>>& lightPosition) {
    if (program.coordinateCount > 8 || program.coordinates.size() != program.coordinateCount)
        throw std::runtime_error("Texture generation requires a complete coordinate program.");
    TextureCoordinates result;
    unsigned expected = 0;
    for (const auto& coordinate : program.coordinates) {
        if (coordinate.index != expected++) throw std::runtime_error("Invalid texture coordinate ordering.");
        auto& output = result.coordinates[coordinate.index];
        output.source = {coordinate.type, coordinate.source, 0, 0};
        if (coordinate.type == 1) {
            if (coordinate.source != 1 && (coordinate.source < 4 || coordinate.source > 7))
                throw std::runtime_error("Unsupported model texture coordinate source.");
            if (coordinate.matrix == 60 && coordinate.generatedMatrix == -1) {
                output.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            } else {
                if (coordinate.matrix != 30 + coordinate.index * 3 || coordinate.generatedMatrix < 0)
                    throw std::runtime_error("Unsupported model texture matrix binding.");
                const auto found = matrices.find(coordinate.generatedMatrix);
                if (found == matrices.end()) throw std::runtime_error("Model texture matrix has not been evaluated.");
                output.matrix = found->second;
            }
        } else if (coordinate.type == 2 && coordinate.source >= 12 && coordinate.source - 12 < coordinate.index &&
            coordinate.matrix == 60 && coordinate.generatedMatrix == -1) {
            if (!lightPosition) throw std::runtime_error("Bump coordinates require the original view-space light position.");
            output.source[1] -= 12;
            result.lightPosition = {(*lightPosition)[0], (*lightPosition)[1], (*lightPosition)[2], 1};
        } else {
            throw std::runtime_error("Unsupported model texture coordinate generation.");
        }
    }
    return result;
}

void modelrenderer::Geometry::load(ID3D11Device* device, const doors::ModelResource& model) {
    if (!device) throw std::runtime_error("Model geometry upload requires a D3D device.");
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0) throw std::runtime_error("Model rendering requires Direct3D feature level 11.");
    Geometry result;
    result.owner = device;
    ComPtr<ID3DBlob> code, errors;
    const std::string shaderText = std::string(modellighting::shaderSource()) + vertexSource;
    const auto compiled = D3DCompile(shaderText.data(), shaderText.size(), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, code.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(compiled)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Model vertex shader compilation failed.");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, result.shader.GetAddressOf()), "Cannot create model vertex shader.");
    using Vertex = modeldraw::Vertex;
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normals), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normals) + 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normals) + 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, coordinates), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, coordinates) + 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, coordinates) + 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, coordinates) + 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    check(device->CreateInputLayout(elements, 9, code->GetBufferPointer(), code->GetBufferSize(), result.layout.GetAddressOf()), "Cannot create model vertex layout.");
    static_assert(sizeof(VertexMatrices) == 160);
    D3D11_BUFFER_DESC constantDescription{};
    constantDescription.ByteWidth = sizeof(VertexMatrices);
    constantDescription.Usage = D3D11_USAGE_DEFAULT;
    constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&constantDescription, nullptr, result.constants.GetAddressOf()), "Cannot create model matrix buffer.");
    static_assert(sizeof(TextureCoordinates) == 528);
    constantDescription.ByteWidth = sizeof(TextureCoordinates);
    check(device->CreateBuffer(&constantDescription, nullptr, result.textureConstants.GetAddressOf()), "Cannot create model texture coordinate buffer.");
    static_assert(sizeof(modellighting::Inputs) == 832);
    constantDescription.ByteWidth = sizeof(modellighting::Inputs);
    check(device->CreateBuffer(&constantDescription, nullptr, result.lightingConstants.GetAddressOf()), "Cannot create model lighting buffer.");
    for (const auto& entry : model.geometry) {
        auto& batch = result.batches[entry.first];
        for (const auto& source : entry.second) {
            Primitive primitive;
            const auto count = source.vertices.size();
            if (count > std::numeric_limits<UINT>::max() / sizeof(Vertex)) throw std::runtime_error("Model primitive exceeds D3D buffer capacity.");
            std::vector<std::uint32_t> indices;
            switch (source.command) {
            case 0x80:
                primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                for (unsigned i = 0; i + 3 < count; i += 4) indices.insert(indices.end(), {i, i + 1, i + 2, i, i + 2, i + 3});
                break;
            case 0xa0:
                primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                for (unsigned i = 1; i + 1 < count; ++i) indices.insert(indices.end(), {0, i, i + 1});
                break;
            case 0x90: primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
            case 0x98: primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
            case 0xa8: primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
            case 0xb0: primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
            case 0xb8: primitive.topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
            default: throw std::runtime_error("Unsupported model primitive topology.");
            }
            if (!count || ((source.command == 0x80 || source.command == 0xa0) && indices.empty())) continue;
            D3D11_BUFFER_DESC buffer{};
            buffer.ByteWidth = static_cast<UINT>(count * sizeof(Vertex));
            buffer.Usage = D3D11_USAGE_IMMUTABLE;
            buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data{source.vertices.data(), 0, 0};
            check(device->CreateBuffer(&buffer, &data, primitive.vertices.GetAddressOf()), "Cannot upload original model vertices.");
            primitive.count = static_cast<unsigned>(count);
            if (!indices.empty()) {
                buffer.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
                buffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
                data.pSysMem = indices.data();
                check(device->CreateBuffer(&buffer, &data, primitive.indices.GetAddressOf()), "Cannot upload model primitive indices.");
                primitive.count = static_cast<unsigned>(indices.size());
            }
            batch.push_back(std::move(primitive));
        }
    }
    *this = std::move(result);
}

void modelrenderer::Geometry::draw(ID3D11DeviceContext* context, std::int16_t batch, const VertexMatrices& matrices,
    const TextureCoordinates& coordinates, const modellighting::Inputs& lighting, bool cullAllTriangles) const {
    if (!context || !owner) throw std::runtime_error("Model geometry is not ready for drawing.");
    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("Model geometry context belongs to another device.");
    const auto& primitives = batches.at(batch);
    context->UpdateSubresource(constants.Get(), 0, nullptr, &matrices, 0, 0);
    context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
    context->UpdateSubresource(textureConstants.Get(), 0, nullptr, &coordinates, 0, 0);
    context->VSSetConstantBuffers(2, 1, textureConstants.GetAddressOf());
    context->UpdateSubresource(lightingConstants.Get(), 0, nullptr, &lighting, 0, 0);
    context->VSSetConstantBuffers(3, 1, lightingConstants.GetAddressOf());
    context->VSSetShader(shader.Get(), nullptr, 0);
    context->IASetInputLayout(layout.Get());
    const UINT stride = sizeof(modeldraw::Vertex), offset = 0;
    for (const auto& primitive : primitives) {
        if (cullAllTriangles && (primitive.topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST ||
            primitive.topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)) continue;
        context->IASetVertexBuffers(0, 1, primitive.vertices.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(primitive.topology);
        context->IASetIndexBuffer(primitive.indices.Get(), DXGI_FORMAT_R32_UINT, 0);
        if (primitive.indices) context->DrawIndexed(primitive.count, 0, 0);
        else context->Draw(primitive.count, 0);
    }
}
