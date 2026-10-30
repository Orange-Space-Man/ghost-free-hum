#include "platform/d3d11/j3dvertexrenderer.h"
#include "jlib/j3d/j3dshapevertices.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace {
    struct Coordinate {
        std::array<unsigned, 4> control{};
        std::array<std::array<float, 4>, 3> matrix{};
    };
    struct Constants {
        j3dmodel::Matrix position;
        std::array<std::array<float, 4>, 3> normal;
        std::array<float, 16> projection;
        std::array<Coordinate, 8> coordinates;
        std::array<unsigned, 4> counts;
    };
    void check(HRESULT result, const char* message) { if (FAILED(result)) throw std::runtime_error(message); }
    static_assert(sizeof(Constants) == 688);
    const char vertexSource[] = R"(
struct Coordinate { uint4 control; float4 transform[3]; };
cbuffer Transform : register(b0) {
    float4 positionMatrix[3]; float4 normalMatrix[3]; float4 projection[4];
    Coordinate coordinates[8]; uint4 counts;
};
struct Input { float3 position:POSITION; float3 normal:NORMAL0; float3 binormal:NORMAL1; float3 tangent:NORMAL2; float2 uv[8]:TEXCOORD0; };
struct Output {
    float4 position:SV_POSITION; float4 color:COLOR0;
    float3 normal:NORMAL0; float3 binormal:NORMAL1; float3 tangent:NORMAL2;
    float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1; float2 uv2:TEXCOORD2; float2 uv3:TEXCOORD3;
    float3 viewPosition:TEXCOORD4; float2 generated[8]:TEXCOORD5; float4 secondaryColor:COLOR1;
    float4 generatedQ[2]:TEXCOORD13;
};
float3 normalTransform(float3 value) {
    return float3(dot(normalMatrix[0].xyz,value),dot(normalMatrix[1].xyz,value),dot(normalMatrix[2].xyz,value));
}
Output main(Input input) {
    Output output=(Output)0;
    float4 position=float4(input.position,1);
    float4 view=float4(dot(positionMatrix[0],position),dot(positionMatrix[1],position),dot(positionMatrix[2],position),1);
    output.position=float4(dot(projection[0],view),dot(projection[1],view),dot(projection[2],view),dot(projection[3],view));
    output.viewPosition=view.xyz;
    output.normal=normalTransform(input.normal); output.binormal=normalTransform(input.binormal); output.tangent=normalTransform(input.tangent);
    float3 normal=normalize(output.normal);
    if(counts.y>0) output.color=float4(illuminate(0,view.xyz,normal,255).rgb,illuminate(2,view.xyz,normal,255).a)/255.0;
    if(counts.y>1) output.secondaryColor=float4(illuminate(1,view.xyz,normal,255).rgb,illuminate(3,view.xyz,normal,255).a)/255.0;
    output.uv0=input.uv[0]; output.uv1=input.uv[1]; output.uv2=input.uv[2]; output.uv3=input.uv[3];
    for(uint i=0;i<counts.x;++i) {
        uint type=coordinates[i].control.x, origin=coordinates[i].control.y;
        output.generatedQ[i/4][i%4]=1;
        if(type<=1) {
            float4 value;
            if(origin==0) value=float4(input.position,1);
            else if(origin==1) value=float4(input.normal,1);
            else if(origin==2) value=float4(input.binormal,1);
            else if(origin==3) value=float4(input.tangent,1);
            else value=float4(input.uv[origin-4],1,1);
            output.generated[i]=float2(dot(coordinates[i].transform[0],value),dot(coordinates[i].transform[1],value));
            if(type==0) output.generatedQ[i/4][i%4]=dot(coordinates[i].transform[2],value);
        } else {
            float3 direction=normalize(lights[type-2].position.xyz-view.xyz);
            output.generated[i]=output.generated[origin-12]+float2(dot(direction,output.tangent),dot(direction,output.binormal));
        }
    }
    return output;
}
)";
}

modellighting::Inputs j3drenderer::prepareLighting(const j3dmodel::MaterialState& state) {
    modellighting::Inputs result;
    for (unsigned i = 0; i < 2; ++i) for (unsigned c = 0; c < 4; ++c) {
        result.material[i][c] = state.materialColors[i][c]; result.ambient[i][c] = state.ambientColors[i][c];
    }
    unsigned loaded = 0;
    for (unsigned i = 0; i < 8; ++i) if (state.lights[i]) {
        loaded |= 1U << i;
        const auto& value = *state.lights[i];
        auto& light = result.lights[i];
        std::copy(value.color.begin(), value.color.end(), light.color.begin());
        std::copy(value.position.begin(), value.position.end(), light.position.begin());
        std::copy(value.direction.begin(), value.direction.end(), light.direction.begin());
        std::copy_n(value.attenuation.begin(), 3, light.angular.begin());
        std::copy_n(value.attenuation.begin() + 3, 3, light.distance.begin());
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (i % 2 >= state.colorChannels) continue;
        const auto& channel = state.channels[i];
        if (channel.ambientSource || channel.materialSource) throw std::runtime_error("J3D vertex-color lighting requires a color vertex stream.");
        if (channel.enabled > 1 || channel.diffuse > 2 || channel.attenuation > 2 || channel.lights > 255 ||
            (channel.enabled && (channel.lights & ~loaded))) throw std::runtime_error("J3D lighting references invalid channels or unloaded lights.");
        result.channels[i].control = {channel.enabled, channel.ambientSource, channel.materialSource, channel.lights};
        result.channels[i].functions = {channel.attenuation == 0 ? 0 : channel.diffuse, channel.attenuation, 0, 0};
    }
    return result;
}

void j3drenderer::VertexShader::load(ID3D11Device* device) {
    if (!device) throw std::runtime_error("J3D vertex shader requires a device.");
    VertexShader result;
    result.owner = device;
    const std::string text = std::string(modellighting::shaderSource()) + vertexSource;
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    if (FAILED(D3DCompile(text.data(), text.size(), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS,
        0, code.GetAddressOf(), errors.GetAddressOf())))
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Cannot compile J3D vertex shader.");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, result.shader.GetAddressOf()), "Cannot create J3D vertex shader.");
    using Vertex = j3dmodel::ShapeVertex;
    std::vector<D3D11_INPUT_ELEMENT_DESC> elements{{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0}};
    for (unsigned i = 0; i < 3; ++i) elements.push_back({"NORMAL", i, DXGI_FORMAT_R32G32B32_FLOAT, 0,
        unsigned(offsetof(Vertex, normals)) + i * 12, D3D11_INPUT_PER_VERTEX_DATA, 0});
    for (unsigned i = 0; i < 8; ++i) elements.push_back({"TEXCOORD", i, DXGI_FORMAT_R32G32_FLOAT, 0,
        unsigned(offsetof(Vertex, coordinates)) + i * 8, D3D11_INPUT_PER_VERTEX_DATA, 0});
    check(device->CreateInputLayout(elements.data(), unsigned(elements.size()), code->GetBufferPointer(), code->GetBufferSize(), result.layout.GetAddressOf()), "Cannot create J3D vertex layout.");
    D3D11_BUFFER_DESC buffer{};
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer.ByteWidth = sizeof(Constants);
    check(device->CreateBuffer(&buffer, nullptr, result.constants.GetAddressOf()), "Cannot create J3D transform buffer.");
    buffer.ByteWidth = sizeof(modellighting::Inputs);
    check(device->CreateBuffer(&buffer, nullptr, result.lighting.GetAddressOf()), "Cannot create J3D lighting buffer.");
    *this = std::move(result);
}

void j3drenderer::VertexShader::bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state,
    const j3dmodel::ShapePacket& packet, std::size_t matrixCount, const j3dmodel::ShapeGroup& group, const std::array<float, 16>& projection) const {
    if (!context || !owner || !packet.drawMatrices || !packet.normalMatrices || group.matrixKind != 0 || group.matrix >= matrixCount)
        throw std::runtime_error("J3D vertex binding requires an owned rigid matrix group.");
    bindMatrices(context, state, packet.drawMatrices[group.matrix], packet.normalMatrices[group.matrix], projection);
}

void j3drenderer::VertexShader::bindMatrices(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state,
    const j3dmodel::Matrix& position, const j3dmodel::NormalMatrix& normal, const std::array<float, 16>& projection) const {
    if (!context || !owner) throw std::runtime_error("J3D vertex matrices require an initialized renderer.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("J3D vertex context belongs to another device.");
    if (state.textureCoordinates > 8 || state.colorChannels > 2) throw std::runtime_error("Invalid J3D vertex channel counts.");
    const auto lights = prepareLighting(state);
    Constants data{};
    data.position = position;
    std::array<float, 3> scale{1, 1, 1};
    if (state.nbtScale) for (unsigned i = 0; i < 3; ++i) {
        const auto& bytes = *state.nbtScale;
        const unsigned at = 4 + i * 4;
        const std::uint32_t bits = (std::uint32_t(bytes[at]) << 24) | (std::uint32_t(bytes[at + 1]) << 16) | (std::uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
        std::memcpy(&scale[i], &bits, 4);
    }
    for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
        data.normal[row][column] = normal[row * 3 + column] * scale[column];
    data.projection = projection;
    data.counts = {state.textureCoordinates, state.colorChannels, 0, 0};
    for (unsigned i = 0; i < state.textureCoordinates; ++i) {
        const auto& input = state.coordinates[i];
        const unsigned type = input.info[0], source = input.info[1], matrix = input.info[2];
        if (input.normalize || input.postMatrix != 125) throw std::runtime_error("J3D vertex shader requires an unimplemented post-texture transform.");
        auto& output = data.coordinates[i];
        output.control = {type, source, 0, 0};
        if (type <= 1 && source <= 11) {
            if (matrix == 60) output.matrix = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
            else {
                if (matrix > 61) throw std::runtime_error("Invalid J3D texture matrix row.");
                std::copy_n(state.matrixRows.begin() + matrix, 3, output.matrix.begin());
            }
        } else if (!(type >= 2 && type <= 9 && source >= 12 && source - 12 < i && state.lights[type - 2]))
            throw std::runtime_error("Unsupported J3D texture generator or missing emboss light.");
    }
    context->UpdateSubresource(constants.Get(), 0, nullptr, &data, 0, 0);
    context->UpdateSubresource(lighting.Get(), 0, nullptr, &lights, 0, 0);
    context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
    context->VSSetConstantBuffers(3, 1, lighting.GetAddressOf());
    context->VSSetShader(shader.Get(), nullptr, 0);
    context->IASetInputLayout(layout.Get());
}
