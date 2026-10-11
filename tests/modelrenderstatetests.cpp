#include "platform/d3d11/modelmaterialrenderer.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "platform/d3d11/modelbatchrenderer.h"
#include "game/models/modeldrawmatrices.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;

    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    bool comparison(unsigned function, int a, int b) {
        switch (function) {
        case 0: return false;
        case 1: return a < b;
        case 2: return a == b;
        case 3: return a <= b;
        case 4: return a > b;
        case 5: return a != b;
        case 6: return a >= b;
        default: return true;
        }
    }

    class Fixture {
    public:
        Fixture() {
            require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                device.GetAddressOf(), nullptr, context.GetAddressOf())), "Cannot create lighting/state test device.");
            D3D11_TEXTURE2D_DESC description{};
            description.Width = description.Height = 32;
            description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.BindFlags = D3D11_BIND_RENDER_TARGET;
            require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, color.GetAddressOf())), "Cannot create color target.");
            require(SUCCEEDED(device->CreateRenderTargetView(color.Get(), nullptr, colorView.GetAddressOf())), "Cannot create color view.");
            description.Format = DXGI_FORMAT_D32_FLOAT;
            description.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, depth.GetAddressOf())), "Cannot create depth target.");
            require(SUCCEEDED(device->CreateDepthStencilView(depth.Get(), nullptr, depthView.GetAddressOf())), "Cannot create depth view.");
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, depthReadback.GetAddressOf())), "Cannot create depth readback.");
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, colorReadback.GetAddressOf())), "Cannot create color readback.");
            context->OMSetRenderTargets(1, colorView.GetAddressOf(), depthView.Get());
            D3D11_VIEWPORT viewport{0, 0, 32, 32, 0, 1};
            context->RSSetViewports(1, &viewport);
            matrices.modelView = matrices.normal = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            matrices.projection = {1000, 0, 0, 0, 0, 1000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            program = modelmaterial::buildStages({}, 0, 0, false, false);
            pipeline.cull = 0;
            lighting.material[0] = {128, 64, 32, 128};
            upload(false);
        }

        ~Fixture() { context->ClearState(); }

        void upload(bool reverse, bool varyingNormals = false) {
            doors::ModelResource model;
            modeldraw::Primitive primitive;
            primitive.command = 0x80;
            const std::array<std::array<float, 3>, 4> points{{{-0.00075f, -0.00075f, 0.5f},
                {0.00075f, -0.00075f, 0.5f}, {0.00075f, 0.00075f, 0.5f}, {-0.00075f, 0.00075f, 0.5f}}};
            for (unsigned i = 0; i < 4; ++i) {
                modeldraw::Vertex vertex;
                vertex.position = points[reverse ? 3 - i : i];
                vertex.normals[0] = {0, 0, varyingNormals && i >= 2 ? -1.0f : 1.0f};
                vertex.color = {60, 80, 100, 120};
                primitive.vertices.push_back(vertex);
            }
            model.geometry[0].push_back(primitive);
            geometry.load(device.Get(), model);
        }

        void clear(float depthValue = 1) {
            const float background[4] = {16 / 255.0f, 32 / 255.0f, 48 / 255.0f, 64 / 255.0f};
            context->ClearRenderTargetView(colorView.Get(), background);
            context->ClearDepthStencilView(depthView.Get(), D3D11_CLEAR_DEPTH, depthValue, 0);
        }

        void draw() {
            shader.load(device.Get(), program, {}, pipeline);
            shader.draw(context.Get(), {}, geometry, 0, matrices, {}, lighting);
        }

        std::array<int, 4> pixel() {
            context->CopyResource(colorReadback.Get(), color.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            require(SUCCEEDED(context->Map(colorReadback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot map state-test color.");
            const auto* bytes = static_cast<const std::uint8_t*>(mapped.pData) + 16 * mapped.RowPitch + 64;
            const std::array<int, 4> result{bytes[0], bytes[1], bytes[2], bytes[3]};
            context->Unmap(colorReadback.Get(), 0);
            return result;
        }

        float depthPixel() {
            context->CopyResource(depthReadback.Get(), depth.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            require(SUCCEEDED(context->Map(depthReadback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot map state-test depth.");
            const auto* row = reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(mapped.pData) + 16 * mapped.RowPitch);
            const float result = row[16];
            context->Unmap(depthReadback.Get(), 0);
            return result;
        }

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        modelrenderer::PipelineState pipeline;
        modelrenderer::VertexMatrices matrices;
        modellighting::Inputs lighting;
        modelmaterial::StageProgram program;

    private:
        ComPtr<ID3D11Texture2D> color, depth, colorReadback, depthReadback;
        ComPtr<ID3D11RenderTargetView> colorView;
        ComPtr<ID3D11DepthStencilView> depthView;
        modelrenderer::Geometry geometry;
        modelrenderer::MaterialShader shader;
    };

    void expect(const std::array<int, 4>& actual, const std::array<int, 4>& expected, const char* message, int tolerance = 0) {
        for (unsigned i = 0; i < 4; ++i) {
            if (std::abs(actual[i] - expected[i]) > tolerance) {
                std::cerr << message << ": component " << i << " was " << actual[i] << ", expected " << expected[i] << '\n';
                throw std::runtime_error(message);
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
        Fixture fixture;
        const std::array<int, 4> background{16, 32, 48, 64}, surface{128, 64, 32, 128};
        for (unsigned function = 0; function < 8; ++function) {
            for (unsigned reference : {127, 128, 129}) {
                fixture.pipeline.alpha = {function, reference, 0, 7, 0};
                fixture.clear();
                fixture.draw();
                expect(fixture.pixel(), comparison(function, 128, reference) ? surface : background, "Alpha comparison");
            }
        }
        for (unsigned operation = 0; operation < 4; ++operation) {
            for (unsigned bits = 0; bits < 4; ++bits) {
                const bool a = (bits & 1) != 0, b = (bits & 2) != 0;
                fixture.pipeline.alpha = {a ? 7U : 0U, 0, operation, b ? 7U : 0U, 0};
                fixture.clear();
                fixture.draw();
                const bool result = operation == 0 ? a && b : operation == 1 ? a || b : operation == 2 ? a != b : a == b;
                expect(fixture.pixel(), result ? surface : background, "Alpha boolean operation");
            }
        }
        fixture.pipeline.alpha = {};
        for (unsigned function = 0; function < 8; ++function) {
            for (int stored : {1, 2, 3}) {
                fixture.pipeline.depthComparison = function;
                fixture.clear(stored * 0.25f);
                fixture.draw();
                const bool pass = comparison(function, 2, stored);
                expect(fixture.pixel(), pass ? surface : background, "Depth comparison");
                require(fixture.depthPixel() == (pass ? 0.5f : stored * 0.25f), "Depth comparison write differs.");
            }
        }
        fixture.pipeline.depthComparison = 3;
        fixture.pipeline.depthWrite = false;
        fixture.clear(); fixture.draw();
        require(fixture.depthPixel() == 1, "Disabled depth write changed the target.");
        fixture.pipeline.depthWrite = true;
        fixture.pipeline.depthEnabled = false;
        fixture.clear(0); fixture.draw();
        expect(fixture.pixel(), surface, "Disabled depth test");
        require(fixture.depthPixel() == 0, "Disabled depth testing must also disable depth writes.");
        fixture.pipeline.depthEnabled = true;
        fixture.pipeline.alpha = {0, 0, 0, 7, 0};
        for (bool early : {false, true}) {
            fixture.pipeline.earlyDepth = early;
            fixture.clear(); fixture.draw();
            expect(fixture.pixel(), background, "Rejected alpha color write");
            require(fixture.depthPixel() == (early ? 0.5f : 1.0f), "Early depth must survive alpha rejection; late depth must not.");
        }
        fixture.pipeline.earlyDepth = false;
        fixture.pipeline.alpha = {};
        for (bool reverse : {false, true}) {
            fixture.upload(reverse);
            for (unsigned cull = 0; cull < 4; ++cull) {
                fixture.pipeline.cull = cull;
                fixture.clear(); fixture.draw();
                const bool visible = cull == 0 || (reverse ? cull == 2 : cull == 1);
                expect(fixture.pixel(), visible ? surface : background, "Original front/back culling");
                require(fixture.depthPixel() == (visible ? 0.5f : 1.0f), "Culled polygons must not write depth.");
            }
        }
        fixture.upload(false);
        fixture.pipeline.cull = 0;
        fixture.pipeline.blendEnabled = true;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {72, 48, 40, 96}, "Source-alpha blending", 1);
        for (bool source : {false, true}) for (unsigned factor = 0; factor < 8; ++factor) {
            fixture.pipeline.sourceBlend = source ? factor : 0;
            fixture.pipeline.destinationBlend = source ? 0 : factor;
            fixture.clear(); fixture.draw();
            std::array<int, 4> expected{};
            for (unsigned component = 0; component < 4; ++component) {
                const int other = source ? background[component] : surface[component];
                const int weight = factor == 0 ? 0 : factor == 1 ? 255 : factor == 2 ? other : factor == 3 ? 255 - other :
                    factor == 4 ? surface[3] : factor == 5 ? 255 - surface[3] : factor == 6 ? background[3] : 255 - background[3];
                expected[component] = int(std::lround((source ? surface[component] : background[component]) * weight / 255.0));
            }
            expect(fixture.pixel(), expected, "Original blend-factor selection", 1);
        }
        fixture.pipeline.sourceBlend = 4;
        fixture.pipeline.destinationBlend = 5;
        fixture.pipeline.subtract = true;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {0, 0, 16, 0}, "Reverse-subtract blending");
        fixture.pipeline.subtract = false;
        fixture.pipeline.blendEnabled = false;
        fixture.pipeline.colorWrite = false;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {16, 32, 48, 128}, "RGB write mask");
        fixture.pipeline.colorWrite = true;
        fixture.pipeline.alphaWrite = false;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {128, 64, 32, 64}, "Alpha write mask");
        fixture.pipeline.alphaWrite = true;
        fixture.pipeline.targetHasAlpha = false;
        fixture.pipeline.blendEnabled = true;
        fixture.pipeline.sourceBlend = 6;
        fixture.pipeline.destinationBlend = 0;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {128, 64, 32, 64}, "Absent destination alpha must behave as one");
        fixture.pipeline.targetHasAlpha = true;
        fixture.pipeline.blendEnabled = false;
        fixture.lighting.channels[0].control[2] = fixture.lighting.channels[2].control[2] = 1;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {60, 80, 100, 120}, "Vertex material source");
        fixture.lighting.channels[0].control = {1, 0, 0, 1};
        fixture.lighting.channels[0].functions = {2, 2, 0, 0};
        fixture.lighting.channels[2].control = {};
        fixture.lighting.ambient[0] = {10, 20, 30, 40};
        auto& light = fixture.lighting.lights[0];
        light.position = {0, 0, 1.5f, 0};
        light.color = {90, 80, 70, 60};
        fixture.matrices.normal[10] = 3;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {50, 25, 12, 128}, "Diffuse lighting, ambient accumulation and normalized lighting normal");
        fixture.lighting.channels[0].control[3] = 0;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {5, 5, 3, 128}, "Light mask exclusion");
        fixture.lighting.channels[0].control[3] = 1;
        light.position[2] = -0.5f;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {5, 5, 3, 128}, "Clamped back-facing diffuse");
        fixture.lighting.channels[0].functions[0] = 1;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {0, 0, 0, 128}, "Signed diffuse before accumulator clamp");
        fixture.lighting.channels[0].functions = {0, 1, 0, 0};
        light.position[2] = 1.5f;
        light.direction = {0, 0, 1, 0};
        light.angular = {1, 0, 0, 0};
        light.distance = {2, 0, 0, 0};
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {27, 15, 8, 128}, "Spot/distance attenuation");
        fixture.lighting.channels[0].functions = {0, 0, 0, 0};
        light.angular = {0, 0, 1, 0};
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {27, 15, 8, 128}, "Specular attenuation");
        fixture.lighting.channels[0].control = {};
        fixture.lighting.channels[1].control = {1, 0, 0, 2};
        fixture.lighting.channels[1].functions = {0, 2, 0, 0};
        fixture.lighting.lights[1] = light;
        fixture.lighting.lights[1].color = {20, 30, 40, 50};
        fixture.lighting.material[1] = {255, 255, 255, 255};
        fixture.program = modelmaterial::buildStages({}, 0, 0, true, false);
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {147, 93, 71, 128}, "Secondary per-vertex lighting reaches the specular combiner");
        fixture.program = modelmaterial::buildStages({}, 0, 0, false, false);
        fixture.lighting.channels[0].control = {1, 0, 0, 1};
        fixture.lighting.channels[0].functions = {2, 2, 0, 0};
        fixture.lighting.material[0] = {255, 255, 255, 128};
        fixture.lighting.ambient[0] = {};
        fixture.lighting.lights[0].position = {0, 0, 100000, 0};
        fixture.lighting.lights[0].color = {255, 255, 255, 255};
        fixture.upload(false, true);
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {133, 133, 133, 128}, "Lighting must interpolate vertex results rather than light interpolated normals", 1);
        fixture.lighting.channels[2].control = {1, 0, 0, 2};
        fixture.lighting.channels[2].functions = {0, 2, 0, 0};
        fixture.lighting.ambient[0][3] = 10;
        fixture.lighting.lights[1].color[3] = 40;
        fixture.clear(); fixture.draw();
        expect(fixture.pixel(), {133, 133, 133, 25}, "Independent alpha lighting channel", 1);

        modellighting::State owner;
        originalmath::FloatTrig trig;
        const dooranimation::Matrix view{1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30};
        modellighting::initialize(owner, view, {}, false, {10, 20, 30, 40}, trig);
        require(owner.lights[0].position == std::array<float, 4>{10, 10020, 10030, 0} &&
            owner.material.lightMask == 1 && owner.material.attenuation == 2, "Original fallback light setup differs.");
        modellighting::prepareModel(owner, view, {1, 2, 3}, {4, 6, 8}, false, trig);
        require(owner.lights[7].position == std::array<float, 4>{17, 30, 43, 0} && owner.ambient[1][3] == 255,
            "Original non-spot secondary light placement differs.");
        modelmaterial::Material material;
        material.colorSource = 1;
        material.color = {128, 64, 32, 200};
        material.specular = true;
        material.shininess = 12;
        require(modellighting::configureMaterial(owner, material, 100, 1, 0), "Specular stage selection lost.");
        auto prepared = modellighting::prepare(owner);
        require(prepared.material[0][3] == 100 && prepared.channels[1].functions[0] == 0 &&
            prepared.lights[6].distance == std::array<float, 4>{6, 0, -5, 0}, "Specular setup or opacity cap differs.");
        modellighting::configureMaterial(owner, material, 100, 1, 2);
        require(owner.material.channels[0].enabled == 0 && owner.material.channels[1].enabled == 1,
            "Node lighting override must not suppress the separately selected specular channel.");
        owner.loadedMask &= ~0x80U;
        bool rejected = false;
        try { modellighting::prepare(owner); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Missing secondary light accepted.");
        modelrenderer::PipelineState retained;
        retained.colorWrite = retained.alphaWrite = false;
        modelrenderer::initializeModelState(retained);
        require(!retained.colorWrite && !retained.alphaWrite && retained.alpha.first == 4 && retained.alpha.operation == 1,
            "Model initialization changed retained write masks or original alpha test.");
        modelrenderer::selectModelPass(retained, 256);
        require(retained.earlyDepth, "Pass selection lost original byte truncation.");
        modelrenderer::selectBatchCull(retained, 1, 1);
        require(retained.cull == 2, "Reflected cull selection differs.");
        modelrenderer::selectBatchCull(retained, 3, 1);
        require(retained.cull == 2, "Reflected cull case should preserve retained state.");
        if (argc > 1) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable;
            std::string error;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) && trig.read(executable, error), error.c_str());
            modellighting::RoomLight room;
            room.position = {1, 2, 3}; room.direction = {0, 0, -1}; room.color = {20, 40, 60, 80};
            room.cutoff = 30; room.referenceDistance = 100; room.brightnessPercent = 50;
            modellighting::initialize(owner, view, {room}, true, {10, 20, 30, 40}, trig);
            require(std::abs(owner.lights[0].distance[1] - 0.01f) < 1e-7f && owner.lights[0].direction[2] == 1 &&
                owner.lights[0].angular[1] < -6, "Original room spot upload differs.");
            room.cutoff = 0;
            modellighting::initialize(owner, view, {room}, true, {}, trig);
            require(std::abs(owner.lights[0].distance[2] - 0.0001f) < 1e-9f, "Original point-light quadratic attenuation differs.");
            modellighting::prepareModel(owner, view, {0, 0, 0}, {0, 0, -10}, true, trig);
            require(owner.ambient[1][3] == 0 && owner.lights[7].direction[2] == 1 &&
                std::abs(owner.lights[7].angular[0] + 866.0254f) < 0.001f, "Original secondary spotlight setup differs.");
            std::vector<std::uint8_t> compressed, archive;
            require(package.read("files/Game/game_usa.szp", compressed, error) && roomarchive::decompress(compressed, archive, error), error.c_str());
            doors::Resources resources;
            require(resources.load(archive, executable, error), error.c_str());
            unsigned submissions = 0;
            for (unsigned modelIndex = 0; modelIndex < 15; ++modelIndex) {
                const auto& model = resources.model(modelIndex);
                modelrenderer::Batches renderer;
                renderer.load(fixture.device.Get(), model);
                for (const auto& node : model.drawing.nodes()) for (const auto& binding : node.second.bindings) {
                    modellighting::initialize(owner, view, {}, false, {64, 64, 64, 255}, trig);
                    modellighting::prepareModel(owner, view, {0, 0, 0}, {0, 0, -10}, false, trig);
                    modelrenderer::PipelineState state;
                    modelrenderer::initializeModelState(state);
                    std::array<modelrenderer::StageRegisters, 16> registers;
                    auto matrices = fixture.matrices;
                    std::array<float, 3> minimum{1e9f, 1e9f, 1e9f}, maximum{-1e9f, -1e9f, -1e9f};
                    for (const auto& primitive : model.geometry.at(binding.batch)) for (const auto& vertex : primitive.vertices)
                        for (unsigned axis = 0; axis < 3; ++axis) {
                            minimum[axis] = std::min(minimum[axis], vertex.position[axis]);
                            maximum[axis] = std::max(maximum[axis], vertex.position[axis]);
                        }
                    matrices.projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        const float scale = (axis == 2 ? 0.5f : 1.5f) / std::max(1.0f, maximum[axis] - minimum[axis]);
                        matrices.modelView[axis * 5] = scale;
                        matrices.modelView[axis * 4 + 3] = -(minimum[axis] + maximum[axis]) * 0.5f * scale + (axis == 2 ? 0.5f : 0);
                    }
                    require(modeldraw::normalMatrix(matrices.modelView, matrices.normal), "Owned fixture produced a singular transform.");
                    const modeldraw::DrawCommand command{node.first, binding.material, binding.batch, 255, 0, false};
                    const auto pass = model.drawing.materialPass(binding.material);
                    fixture.clear();
                    renderer.draw(fixture.context.Get(), command, pass, 0, matrices, trig, owner, state, {}, registers, false);
                    const auto cached = renderer.shaderCount();
                    renderer.draw(fixture.context.Get(), command, pass, 0, matrices, trig, owner, state, {}, registers, false);
                    require(renderer.shaderCount() == cached, "Unchanged original batch rebuilt its shader.");
                    auto faded = command;
                    faded.opacity = 100;
                    renderer.draw(fixture.context.Get(), faded, pass, 0, matrices, trig, owner, state, {}, registers, false);
                    require(renderer.shaderCount() == cached, "Dynamic opacity unnecessarily rebuilt its shader.");
                    bool shadowRejected = false;
                    try { renderer.draw(fixture.context.Get(), command, pass, 0, matrices, trig, owner, state, {}, registers, true); }
                    catch (const std::runtime_error&) { shadowRejected = true; }
                    require(shadowRejected, "Incomplete projected shadow prefix must not be substituted.");
                    ++submissions;
                }
            }
            std::cout << "Submitted " << submissions << " original material/batch bindings through the connected renderer.\n";
            std::cout << "Owned executable trigonometry verified through original room and secondary light setup.\n";
        }
        std::cout << "Lighting, alpha comparisons, early/late depth, blend, write masks and culling rendered checks passed.\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
