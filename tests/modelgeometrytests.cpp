#include "platform/d3d11/modelgeometryrenderer.h"
#include "platform/d3d11/modelmaterialrenderer.h"
#include "platform/d3d11/modeltexturerenderer.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <d3dcompiler.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            device.GetAddressOf(), nullptr, context.GetAddressOf())), "Cannot create geometry test device.");
        const char pixelSource[] = R"(
struct Input {
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
float4 main(Input input) : SV_TARGET {
    return float4(input.normal.x, input.uv2.x, input.uv3.y, input.color.a);
})";
        ComPtr<ID3DBlob> code, errors;
        require(SUCCEEDED(D3DCompile(pixelSource, sizeof(pixelSource), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0,
            code.GetAddressOf(), errors.GetAddressOf())), "Cannot compile geometry fixture pixel shader.");
        ComPtr<ID3D11PixelShader> pixelShader;
        require(SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
            pixelShader.GetAddressOf())), "Cannot create fixture pixel shader.");
        D3D11_TEXTURE2D_DESC targetDescription{};
        targetDescription.Width = targetDescription.Height = 32;
        targetDescription.MipLevels = targetDescription.ArraySize = targetDescription.SampleDesc.Count = 1;
        targetDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        targetDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target, staging;
        require(SUCCEEDED(device->CreateTexture2D(&targetDescription, nullptr, target.GetAddressOf())), "Cannot create render target.");
        targetDescription.BindFlags = 0;
        targetDescription.Usage = D3D11_USAGE_STAGING;
        targetDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        require(SUCCEEDED(device->CreateTexture2D(&targetDescription, nullptr, staging.GetAddressOf())), "Cannot create geometry readback.");
        ComPtr<ID3D11RenderTargetView> targetView;
        require(SUCCEEDED(device->CreateRenderTargetView(target.Get(), nullptr, targetView.GetAddressOf())), "Cannot create render target view.");
        context->OMSetRenderTargets(1, targetView.GetAddressOf(), nullptr);
        D3D11_VIEWPORT viewport{0, 0, 32, 32, 0, 1};
        context->RSSetViewports(1, &viewport);
        D3D11_RASTERIZER_DESC rasterDescription{};
        rasterDescription.FillMode = D3D11_FILL_SOLID;
        rasterDescription.CullMode = D3D11_CULL_NONE;
        rasterDescription.DepthClipEnable = true;
        ComPtr<ID3D11RasterizerState> raster;
        require(SUCCEEDED(device->CreateRasterizerState(&rasterDescription, raster.GetAddressOf())), "Cannot create fixture rasterizer.");
        context->RSSetState(raster.Get());
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        modellighting::Inputs lighting;
        for (auto& channel : lighting.channels) channel.control[2] = 1;
        modelrenderer::PipelineState pipeline;
        pipeline.cull = 0;
        modelrenderer::VertexMatrices matrices;
        matrices.modelView = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        matrices.normal = {2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        matrices.projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const auto readCenter = [&]() {
            context->CopyResource(staging.Get(), target.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot map geometry readback.");
            const auto* data = static_cast<const std::uint8_t*>(mapped.pData) + 16 * mapped.RowPitch + 16 * 4;
            std::array<unsigned, 4> result{data[0], data[1], data[2], data[3]};
            context->Unmap(staging.Get(), 0);
            return result;
        };
        const float clear[4]{};
        modelrenderer::Geometry geometry;
        modelmaterial::StageProgram ordinaryProgram;
        ordinaryProgram.coordinateCount = 4;
        for (unsigned i = 0; i < 4; ++i) ordinaryProgram.coordinates.push_back({i, 1, i + 4, 60, -1});
        const auto ordinaryCoordinates = modelrenderer::prepareCoordinates(ordinaryProgram, {});
        for (unsigned command : {0x80, 0x90, 0x98, 0xa0}) {
            context->PSSetShader(pixelShader.Get(), nullptr, 0);
            doors::ModelResource model;
            modeldraw::Primitive primitive;
            primitive.command = static_cast<std::uint8_t>(command);
            const std::array<std::array<float, 3>, 4> positions{{{-0.75f, -0.75f, 0.5f}, {0.75f, -0.75f, 0.5f},
                {0.75f, 0.75f, 0.5f}, {-0.75f, 0.75f, 0.5f}}};
            std::vector<unsigned> order = command == 0x90 ? std::vector<unsigned>{0, 1, 2, 0, 2, 3} :
                command == 0x98 ? std::vector<unsigned>{0, 1, 3, 2} : std::vector<unsigned>{0, 1, 2, 3};
            for (unsigned index : order) {
                modeldraw::Vertex vertex;
                vertex.position = positions[index];
                vertex.normals[0] = {0.25f, 0, 0};
                vertex.color = {255, 255, 255, 128};
                vertex.coordinates[2] = {0.25f, 0};
                vertex.coordinates[3] = {0, 0.75f};
                primitive.vertices.push_back(vertex);
            }
            model.geometry[0].push_back(primitive);
            geometry.load(device.Get(), model);
            context->ClearRenderTargetView(targetView.Get(), clear);
            geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
            const auto pixel = readCenter();
            require(std::abs(int(pixel[0]) - 128) <= 1 && std::abs(int(pixel[1]) - 64) <= 1 &&
                std::abs(int(pixel[2]) - 191) <= 1 && pixel[3] == 128, "Primitive rendering lost transformed normals, UV streams or vertex alpha.");
            auto moved = matrices;
            moved.modelView[3] = 2;
            context->ClearRenderTargetView(targetView.Get(), clear);
            geometry.draw(context.Get(), 0, moved, ordinaryCoordinates, lighting);
            require(readCenter() == std::array<unsigned, 4>{0, 0, 0, 0}, "Model-view translation was not applied before projection.");
            if (command == 0x80) {
                modelmaterial::Material material;
                auto program = modelmaterial::buildStages(material, 0, 0, false, false);
                modelrenderer::MaterialShader shader;
                modelrenderer::CombinerInputs inputs;
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                context->ClearRenderTargetView(targetView.Get(), clear);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{255, 255, 255, 128}, "Untextured original combiner does not preserve raster color and alpha.");
                modelmaterial::Sampler sampler;
                sampler.present = true;
                sampler.width = sampler.height = 1;
                material.samplers.push_back(sampler);
                model.materials[0] = material;
                modelmaterial::TextureImages images;
                images.levels.push_back({1, 1, {128, 64, 32, 64}});
                model.textureImages[0].push_back(images);
                modelrenderer::Textures textures;
                textures.load(device.Get(), model);
                program = modelmaterial::buildStages(material, 1, 0, false, false);
                textures.bind(context.Get(), 0, program);
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{128, 64, 32, 32}, "Textured combiner input selection or integer alpha rounding differs.");
                auto swapped = program;
                swapped.stages[0].resetSwap = false;
                std::array<modelrenderer::StageRegisters, 16> retained;
                retained[0].textureSwap = {2, 1, 0, 3};
                shader.load(device.Get(), swapped, retained, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{32, 64, 128, 32}, "Retained texture swap must apply when the original stage omits a reset.");
                program = modelmaterial::buildStages(material, 1, 0, true, false);
                lighting.channels[1].control[2] = lighting.channels[3].control[2] = 0; lighting.material[1] = {20, 30, 40, 50};
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{148, 94, 72, 32}, "Specular append should add secondary RGB while retaining prior alpha.");
                modelmaterial::Stage first;
                first.color = {15, 15, 15, 2};
                first.alpha = {7, 7, 7, 1};
                first.colorOutput = first.alphaOutput = 2;
                first.colorClamp = first.alphaClamp = false;
                modelmaterial::Stage second;
                second.index = 1;
                second.color = {4, 15, 15, 15};
                second.alpha = {2, 7, 7, 7};
                program = {};
                program.stageCount = 2;
                program.stages = {first, second};
                inputs.registers[1] = {-10, -11, -12, -20};
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{246, 245, 244, 236}, "Unclamped register values must wrap to eight bits when consumed as A/B/C inputs.");
                second.color = {15, 15, 15, 4};
                second.alpha = {7, 7, 7, 2};
                second.colorOperation = second.alphaOperation = 1;
                program.stages[1] = second;
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{0, 0, 0, 0}, "Signed D inputs must remain negative until clamping.");
                first.color = {0, 15, 15, 2};
                first.alpha = {0, 7, 7, 1};
                first.colorOutput = first.alphaOutput = 0;
                first.colorOperation = first.alphaOperation = 1;
                first.colorClamp = first.alphaClamp = true;
                program.stages = {first};
                program.stageCount = 1;
                inputs.registers[0] = {20, 30, 40, 50};
                inputs.registers[1] = {100, 100, 100, 100};
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), inputs);
                geometry.draw(context.Get(), 0, matrices, ordinaryCoordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{80, 70, 60, 50}, "Subtraction must use pre-stage color and alpha register values.");
                for (auto& vertex : model.geometry[0][0].vertices) {
                    vertex.normals[1] = {0, 0, 0.25f};
                    vertex.normals[2] = {0, 0, 0.125f};
                }
                geometry.load(device.Get(), model);
                std::string coordinateSource = pixelSource;
                const std::string oldReturn = "return float4(input.normal.x, input.uv2.x, input.uv3.y, input.color.a);";
                coordinateSource.replace(coordinateSource.find(oldReturn), oldReturn.size(),
                    "return float4(input.generated[0], input.generated[1]);");
                code.Reset();
                errors.Reset();
                require(SUCCEEDED(D3DCompile(coordinateSource.data(), coordinateSource.size(), nullptr, nullptr, nullptr,
                    "main", "ps_4_0", 0, 0, code.GetAddressOf(), errors.GetAddressOf())), "Cannot compile coordinate fixture.");
                ComPtr<ID3D11PixelShader> coordinateShader;
                require(SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                    coordinateShader.GetAddressOf())), "Cannot create coordinate fixture.");
                program = {};
                program.coordinateCount = 2;
                program.coordinates = {{0, 1, 1, 30, 7}, {1, 2, 12, 60, -1}};
                const std::map<int, dooranimation::Matrix> textureMatrices{
                    {7, {2, 0, 0, 0.25f, 0, 1, 0, 0.25f, 0, 0, 1, 0}}};
                bool rejected = false;
                try { modelrenderer::prepareCoordinates(program, textureMatrices); }
                catch (const std::runtime_error&) { rejected = true; }
                require(rejected, "Bump generation accepted a missing original light.");
                rejected = false;
                try { modelrenderer::prepareCoordinates(program, {}, std::array<float, 3>{0, 0, 10000}); }
                catch (const std::runtime_error&) { rejected = true; }
                require(rejected, "Generated coordinates accepted an unevaluated texture matrix.");
                auto coordinates = modelrenderer::prepareCoordinates(program, textureMatrices, std::array<float, 3>{0, 0, 10000});
                context->PSSetShader(coordinateShader.Get(), nullptr, 0);
                auto bumpMatrices = matrices;
                bumpMatrices.normal[10] = 2;
                geometry.draw(context.Get(), 0, bumpMatrices, coordinates, lighting);
                auto generated = readCenter();
                require(std::abs(int(generated[0]) - 191) <= 1 && std::abs(int(generated[1]) - 64) <= 1 &&
                    std::abs(int(generated[2]) - 255) <= 1 && std::abs(int(generated[3]) - 191) <= 1,
                    "Generated coordinates lost raw normals, matrix translation, prior coordinates or scaled bump basis.");
                program.coordinates = {{0, 1, 6, 60, -1}, {1, 1, 7, 60, -1}};
                coordinates = modelrenderer::prepareCoordinates(program, {});
                geometry.draw(context.Get(), 0, matrices, coordinates, lighting);
                generated = readCenter();
                require(std::abs(int(generated[0]) - 64) <= 1 && generated[1] == 0 && generated[2] == 0 &&
                    std::abs(int(generated[3]) - 191) <= 1, "Texture generators must select UV sources independently of destination index.");
                material.samplers[0].generatedMatrix = 7;
                material.samplers[0].width = material.samplers[0].height = 2;
                model.materials[0] = material;
                model.textureImages[0][0].levels = {{2, 2, {255, 0, 0, 255, 0, 255, 0, 255,
                    0, 0, 255, 255, 255, 255, 255, 255}}};
                textures.load(device.Get(), model);
                program = modelmaterial::buildStages(material, 1, 0, false, false);
                textures.bind(context.Get(), 0, program);
                shader.load(device.Get(), program, {}, pipeline);
                shader.bind(context.Get(), {});
                coordinates = modelrenderer::prepareCoordinates(program, textureMatrices);
                geometry.draw(context.Get(), 0, matrices, coordinates, lighting);
                require(readCenter() == std::array<unsigned, 4>{0, 255, 0, 128},
                    "Material sampling did not consume vertex-generated texture coordinates.");
            }
        }
        if (argc > 1) {
            gameassets::Package package;
            std::string error;
            std::vector<std::uint8_t> compressed, archive, executable;
            require(package.open(argv[1], error) && package.read("files/Game/game_usa.szp", compressed, error) &&
                roomarchive::decompress(compressed, archive, error) && package.read("sys/main.dol", executable, error), error.c_str());
            doors::Resources resources;
            require(resources.load(archive, executable, error), error.c_str());
            for (unsigned index = 0; index < 15; ++index) geometry.load(device.Get(), resources.model(index));
            std::cout << "Uploaded original geometry from all fifteen owned door models.\n";
        }
        context->ClearState();
        std::cout << "D3D model vertex transforms, primitive assembly and attribute rendering passed.\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
