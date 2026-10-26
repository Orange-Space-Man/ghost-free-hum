#include "platform/d3d11/j3dshaperenderer.h"
#include "platform/d3d11/j3dcombinerrenderer.h"
#include "platform/d3d11/j3dtexturerenderer.h"
#include "platform/d3d11/j3dvertexrenderer.h"

#include <d3dcompiler.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;
    void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void word(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
    void real(std::vector<std::uint8_t>& bytes, unsigned offset, float value) {
        unsigned bits; std::memcpy(&bits, &value, 4); word(bytes, offset, bits);
    }
}

int main() {
    try {
        auto bytes = std::make_shared<std::vector<std::uint8_t>>(512);
        for (const auto& format : {std::array<unsigned, 4>{16, 9, 1, 4}, {32, 10, 0, 3}, {48, 13, 1, 3}}) {
            word(*bytes, format[0], format[1]); word(*bytes, format[0] + 4, format[2]); word(*bytes, format[0] + 8, format[3]);
        }
        (*bytes)[44] = 15; (*bytes)[60] = 8; word(*bytes, 64, 255);
        word(*bytes, 96, 9); word(*bytes, 100, 3);
        word(*bytes, 104, 25); word(*bytes, 108, 2);
        word(*bytes, 112, 13); word(*bytes, 116, 2); word(*bytes, 120, 255);
        (*bytes)[160] = 0x90; (*bytes)[162] = 3;
        const std::array<std::array<float, 3>, 3> positions{{{-0.75f, -0.75f, 0.5f}, {0, 0.75f, 0.5f}, {0.75f, -0.75f, 0.5f}}};
        for (unsigned i = 0; i < 3; ++i) {
            (*bytes)[164 + i * 4] = static_cast<std::uint8_t>(i);
            for (unsigned j = 0; j < 3; ++j) real(*bytes, 256 + i * 12 + j * 4, positions[i][j]);
        }
        (*bytes)[352] = (*bytes)[360] = (*bytes)[368] = 0x40;
        (*bytes)[400] = 0xff; (*bytes)[402] = 2;
        j3dmodel::Geometry model;
        model.bytes = bytes; model.vertexFormatOffset = 16;
        model.vertexOffsets[0] = 256; model.vertexOffsets[1] = 320; model.vertexOffsets[2] = 352; model.vertexOffsets[5] = 400;
        j3dmodel::Shape shape;
        shape.descriptorOffset = 96; shape.groups.push_back({0, 0, {}, 160, 15});
        auto decoded = j3dmodel::decodeShapeGroup(model, shape, shape.groups[0], true);
        check(decoded.size() == 1 && decoded[0].vertices.size() == 3 && decoded[0].vertices[2].position == positions[2],
            "Indexed positions retain big-endian floats and primitive order.");
        const auto& vertex = decoded[0].vertices[0];
        check(vertex.normals == std::array<std::array<float, 3>, 3>{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}} &&
            vertex.coordinates[0] == std::array<float, 2>{-1, 2},
            "NBT overrides format count and array stride, consumes normals before UVs, and retains hardware normal fractions.");
        auto truncated = shape.groups[0]; --truncated.drawSize;
        bool rejected = false;
        try { j3dmodel::decodeShapeGroup(model, shape, truncated, true); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Truncated primitives cannot read into adjacent resource data.");
        (*bytes)[163] = (*bytes)[164] = 255;
        decoded = j3dmodel::decodeShapeGroup(model, shape, shape.groups[0], true);
        check(decoded[0].vertices[0].skipped && decoded[0].vertices[1].position == positions[1],
            "Position sentinel marks a skipped vertex while consuming its remaining attributes.");
        (*bytes)[163] = (*bytes)[164] = 0;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            device.GetAddressOf(), nullptr, context.GetAddressOf())), "Cannot create J3D software rendering device.");
        j3drenderer::ShapeGeometry geometry;
        geometry.load(device.Get(), model, shape, true);
        const char* vertexCode = "struct O{float4 p:SV_POSITION;float2 uv[8]:TEXCOORD5;}; O main(float3 p:POSITION){O o;o.p=float4(p,1);for(int i=0;i<8;++i)o.uv[i]=float2(.5,.5);return o;}";
        const char* pixelCode = "float4 main():SV_TARGET{return float4(1,0,0,1);}";
        ComPtr<ID3DBlob> vsCode, psCode, errors;
        check(SUCCEEDED(D3DCompile(vertexCode, std::strlen(vertexCode), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0,
            vsCode.GetAddressOf(), errors.GetAddressOf())) && SUCCEEDED(D3DCompile(pixelCode, std::strlen(pixelCode), nullptr,
                nullptr, nullptr, "main", "ps_4_0", 0, 0, psCode.GetAddressOf(), errors.ReleaseAndGetAddressOf())), "Cannot compile geometry diagnostics.");
        ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps; ComPtr<ID3D11InputLayout> layout;
        check(SUCCEEDED(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, vs.GetAddressOf())) &&
            SUCCEEDED(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, ps.GetAddressOf())), "Cannot create geometry diagnostic shaders.");
        const D3D11_INPUT_ELEMENT_DESC element{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            offsetof(j3dmodel::ShapeVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0};
        check(SUCCEEDED(device->CreateInputLayout(&element, 1, vsCode->GetBufferPointer(), vsCode->GetBufferSize(), layout.GetAddressOf())), "Cannot create J3D input layout.");
        D3D11_TEXTURE2D_DESC description{};
        description.Width = description.Height = 32; description.MipLevels = description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM; description.SampleDesc.Count = 1;
        description.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output, readback;
        ComPtr<ID3D11RenderTargetView> view;
        check(SUCCEEDED(device->CreateTexture2D(&description, nullptr, output.GetAddressOf())) &&
            SUCCEEDED(device->CreateRenderTargetView(output.Get(), nullptr, view.GetAddressOf())), "Cannot create geometry diagnostic target.");
        description.BindFlags = 0; description.Usage = D3D11_USAGE_STAGING; description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        check(SUCCEEDED(device->CreateTexture2D(&description, nullptr, readback.GetAddressOf())), "Cannot create geometry readback.");
        D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = true;
        ComPtr<ID3D11RasterizerState> rasterState;
        check(SUCCEEDED(device->CreateRasterizerState(&raster, rasterState.GetAddressOf())), "Cannot create diagnostic raster state.");
        auto* target = view.Get(); context->OMSetRenderTargets(1, &target, nullptr);
        const float clear[4]{}; context->ClearRenderTargetView(target, clear);
        const D3D11_VIEWPORT viewport{0, 0, 32, 32, 0, 1}; context->RSSetViewports(1, &viewport); context->RSSetState(rasterState.Get());
        context->IASetInputLayout(layout.Get()); context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
        for (std::uint8_t command : {0x90, 0x98, 0xa0, 0x80}) {
            (*bytes)[160] = command;
            geometry.load(device.Get(), model, shape, true);
            context->ClearRenderTargetView(target, clear);
            geometry.drawGroup(context.Get(), 0);
            context->CopyResource(readback.Get(), output.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read J3D diagnostic pixels.");
            const auto* center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
            const bool red = center[0] == 255 && center[1] == 0 && center[2] == 0 && center[3] == 255;
            context->Unmap(readback.Get(), 0);
            check(red, "Triangles, strips, fans and the original three-vertex quad case must produce triangle pixels.");
        }
        j3dmodel::MaterialState material;
        material.combiner.count = 1;
        material.combiner.colors[1] = {200, 100, 50, 240};
        material.combiner.constants[0] = {128, 128, 128, 128};
        material.combiner.swaps[0] = {0, 1, 2, 3};
        auto& stage = material.combiner.stages[0];
        stage.order = {255, 255, 255};
        stage.colorInputs = {15, 2, 14, 15}; stage.alphaInputs = {7, 1, 6, 7};
        stage.colorOperation = stage.alphaOperation = {0, 0, 0, 1, 0};
        stage.colorSelection = 12; stage.alphaSelection = 28;
        modelrenderer::PipelineState pipeline;
        pipeline.depthEnabled = pipeline.depthWrite = false; pipeline.cull = 0;
        j3drenderer::CombinerShader combiner;
        combiner.load(device.Get(), material, pipeline);
        combiner.bind(context.Get(), material);
        context->ClearRenderTargetView(target, clear);
        geometry.drawGroup(context.Get(), 0);
        context->CopyResource(readback.Get(), output.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read J3D combiner pixels.");
        const auto* center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
        const bool combined = center[0] == 101 && center[1] == 50 && center[2] == 25 && center[3] == 121;
        context->Unmap(readback.Get(), 0);
        check(combined, "J3D retained colors and konst values must execute the integer TEV blend in D3D.");
        material.combiner.count = 2;
        auto& subtract = material.combiner.stages[1];
        subtract = stage;
        subtract.colorInputs[3] = 0; subtract.alphaInputs[3] = 0;
        subtract.colorOperation[0] = subtract.alphaOperation[0] = 1;
        subtract.colorSelection = subtract.alphaSelection = 6;
        combiner.load(device.Get(), material, pipeline);
        combiner.bind(context.Get(), material);
        geometry.drawGroup(context.Get(), 0);
        context->CopyResource(readback.Get(), output.Get());
        check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read subtraction pixels.");
        center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
        const bool subtracted = center[0] == 51 && center[1] == 25 && center[2] == 13 && center[3] == 61;
        context->Unmap(readback.Get(), 0);
        check(subtracted, "Later J3D stages must read earlier outputs and retain subtract rounding.");
        description.Width = description.Height = 1; description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description.Usage = D3D11_USAGE_IMMUTABLE; description.CPUAccessFlags = 0;
        const std::array<std::uint8_t, 4> texel{10, 20, 30, 40};
        D3D11_SUBRESOURCE_DATA data{texel.data(), 4, 0};
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> resource;
        check(SUCCEEDED(device->CreateTexture2D(&description, &data, texture.GetAddressOf())) &&
            SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, resource.GetAddressOf())), "Cannot create combiner texture fixture.");
        D3D11_SAMPLER_DESC sampling{};
        sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        ComPtr<ID3D11SamplerState> sampler;
        check(SUCCEEDED(device->CreateSamplerState(&sampling, sampler.GetAddressOf())), "Cannot create combiner sampler fixture.");
        context->PSSetShaderResources(0, 1, resource.GetAddressOf());
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
        material.combiner.count = material.textureCoordinates = 1;
        stage.order = {0, 0, 255}; stage.colorInputs = {15, 8, 12, 15}; stage.alphaInputs = {7, 4, 6, 7};
        stage.alphaSelection = 0; stage.swap[1] = 1;
        material.combiner.swaps[1] = {2, 1, 0, 3};
        combiner.load(device.Get(), material, pipeline);
        combiner.bind(context.Get(), material);
        geometry.drawGroup(context.Get(), 0);
        context->CopyResource(readback.Get(), output.Get());
        check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read textured combiner pixels.");
        center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
        const bool textured = center[0] == 30 && center[1] == 20 && center[2] == 10 && center[3] == 40;
        context->Unmap(readback.Get(), 0);
        check(textured, "J3D texture sampling and retained channel swaps must reach the output pixel.");
        model.textureHeaders = {416};
        (*bytes)[416] = 4; (*bytes)[419] = 4; (*bytes)[421] = 4;
        (*bytes)[440] = 1; word(*bytes, 444, 32);
        for (unsigned i = 448; i < 480; i += 2) (*bytes)[i] = 0xf8;
        j3drenderer::Textures originalTextures;
        originalTextures.load(device.Get(), model);
        auto modelData = std::make_shared<j3dmodel::ModelData>();
        modelData->geometry.drawMatrices.resize(1);
        j3dmodel::ModelInstance instance(modelData, 0);
        *reinterpret_cast<j3dmodel::Matrix*>(instance.drawMatrices.data()) = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        *reinterpret_cast<j3dmodel::NormalMatrix*>(instance.normalMatrices.data()) = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        j3dmodel::ShapePacket matrixPacket;
        matrixPacket.drawMatrices = reinterpret_cast<j3dmodel::Matrix*>(instance.drawMatrices.data());
        matrixPacket.normalMatrices = reinterpret_cast<j3dmodel::NormalMatrix*>(instance.normalMatrices.data());
        material.colorChannels = 1;
        material.materialColors[0] = {255, 255, 255, 255};
        material.ambientColors[0] = {32, 0, 0, 0};
        material.channels[0] = {1, 0, 0, 1, 2, 2};
        j3dmodel::MaterialLight lamp;
        lamp.position = {1000000, 0, 0}; lamp.color = {128, 0, 0, 255};
        material.light(0, lamp);
        material.coordinates[0].info = {1, 4, 60};
        stage.colorInputs = {15, 8, 10, 15}; stage.alphaInputs = {7, 4, 5, 7}; stage.order[2] = 4; stage.swap[1] = 0;
        originalTextures.bind(context.Get(), material);
        j3drenderer::VertexShader originalVertex;
        originalVertex.load(device.Get());
        const std::array<float, 16> projection{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        originalVertex.bind(context.Get(), material, matrixPacket, 1, shape.groups[0], projection);
        combiner.load(device.Get(), material, pipeline);
        combiner.bind(context.Get(), material);
        geometry.drawGroup(context.Get(), 0);
        context->CopyResource(readback.Get(), output.Get());
        check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read original texture and lighting pixels.");
        center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
        const bool lit = center[0] == 160 && center[1] == 0 && center[2] == 0 && center[3] == 255;
        context->Unmap(readback.Get(), 0);
        check(lit, "Decoded RGB565 texture, rigid matrices, ambient and diffuse vertex lighting must reach the rendered pixel.");
        material.lights[0].reset();
        rejected = false;
        try { originalVertex.bind(context.Get(), material, matrixPacket, 1, shape.groups[0], projection); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Unbound scene lights cannot be replaced by fabricated illumination.");
        for (unsigned row = 0; row < 4; ++row) for (unsigned column = 2; column < 4; ++column) {
            (*bytes)[448 + (row * 4 + column) * 2] = 7;
            (*bytes)[449 + (row * 4 + column) * 2] = 0xe0;
        }
        originalTextures.load(device.Get(), model);
        material.channels[0].enabled = 0;
        lamp.position = {0, 0, 1000000}; material.light(0, lamp);
        material.textureCoordinates = 2;
        material.coordinates[0].info = {1, 4, 30}; material.coordinates[1].info = {2, 12, 60};
        material.matrixRows[30] = {0, 0, 0, 0.125f}; material.matrixRows[31] = {0, 0, 0, 0.5f};
        stage.order = {1, 0, 255}; stage.colorInputs = {15, 8, 12, 15}; stage.alphaInputs = {7, 4, 6, 7};
        std::array<std::uint8_t, 16> nbt{1, 0, 0, 0, 0x3f, 0x80, 0, 0, 0x3f, 0x80, 0, 0, 0, 0, 0, 0};
        material.nbtScale = &nbt;
        originalTextures.bind(context.Get(), material);
        combiner.load(device.Get(), material, pipeline);
        combiner.bind(context.Get(), material);
        for (bool shifted : {false, true}) {
            nbt[12] = shifted ? 0x3f : 0; nbt[13] = shifted ? 0x40 : 0;
            originalVertex.bind(context.Get(), material, matrixPacket, 1, shape.groups[0], projection);
            geometry.drawGroup(context.Get(), 0);
            context->CopyResource(readback.Get(), output.Get());
            check(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read NBT emboss pixels.");
            center = static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * 16 + 16 * 4;
            const bool embossed = center[0] == (shifted ? 0 : 255) && center[1] == (shifted ? 255 : 0) && center[2] == 0;
            context->Unmap(readback.Get(), 0);
            check(embossed, "Original NBT column scaling must control light-driven emboss sampling without normalizing the tangent.");
        }
        std::cout << "J3D vertex decoding, NBT and Direct3D combiner pixel submission verified.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
