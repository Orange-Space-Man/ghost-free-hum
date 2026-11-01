#include "../recovered/playersheetdraw.h"
#include "../recovered/playersheetshadow.h"
#include "../recovered/playersheetrenderer.h"
#include "platform/d3d11/j3dtexturerenderer.h"
#include "game/scene/gameplayresourceowner.h"

#include <stdexcept>
#include <string>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

    void renderCloth(const std::vector<std::array<playersheets::DrawVertex, 4>>& quads,
        j3dmodel::MaterialState state, const playermodels::GridTexture& texture, unsigned slot) {
        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf())), "Cannot create cloth rendering device.");
        D3D11_TEXTURE2D_DESC description{};
        description.Width = description.Height = 32;
        description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output, readback;
        ComPtr<ID3D11RenderTargetView> target;
        require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, output.GetAddressOf())) &&
            SUCCEEDED(device->CreateRenderTargetView(output.Get(), nullptr, target.GetAddressOf())), "Cannot create cloth target.");
        description.BindFlags = 0;
        description.Usage = D3D11_USAGE_STAGING;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, readback.GetAddressOf())), "Cannot create cloth readback.");
        context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
        const D3D11_VIEWPORT viewport{0, 0, 32, 32, 0, 1};
        context->RSSetViewports(1, &viewport);
        const float clear[]{0, 0, 0, 0};
        context->ClearRenderTargetView(target.Get(), clear);
        playersheets::Renderer renderer(device.Get());
        for (unsigned i = 0; i <= slot; ++i) renderer.bindTexture(context.Get(), i, texture);
        ComPtr<ID3D11ShaderResourceView> first, replacement, restored;
        context->PSGetShaderResources(slot, 1, first.GetAddressOf());
        auto changed = texture;
        changed.primary = texture.secondary;
        renderer.bindTexture(context.Get(), slot, changed);
        context->PSGetShaderResources(slot, 1, replacement.GetAddressOf());
        renderer.bindTexture(context.Get(), slot, texture);
        context->PSGetShaderResources(slot, 1, restored.GetAddressOf());
        require(first.Get() != replacement.Get() && first.Get() == restored.Get(),
            "Cloth secondary texture replacement selects a new resource and restores the retained upload without reloading.");
        ComPtr<ID3D11SamplerState> boundSampler;
        context->PSGetSamplers(slot, 1, boundSampler.GetAddressOf());
        D3D11_SAMPLER_DESC sampling{};
        boundSampler->GetDesc(&sampling);
        require(sampling.Filter == D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT && sampling.MaxLOD == 0 &&
            sampling.AddressU == D3D11_TEXTURE_ADDRESS_CLAMP && sampling.AddressV == D3D11_TEXTURE_ADDRESS_CLAMP,
            "Direct cloth binding uses its recovered sampler independently of the model BTI settings.");
        state.ambientColors[0] = {255, 255, 255, 255};
        for (unsigned i : {0U, 2U, 7U}) {
            j3dmodel::MaterialLight light;
            light.position = {0, 0, 10000};
            light.color = {255, 255, 255, 255};
            light.attenuation = {1, 0, 0, 1, 0, 0};
            state.light(i, light);
        }
        state.pixels.alpha = {7, 0, 0, 7, 0};
        state.pixels.depth[0] = state.pixels.depth[2] = 0;
        const actormodel::Matrix camera{1, 0, 0, -0.5f, 0, 0, 1, -0.5f, 0, 1, 0, 0.5f};
        const std::array<float, 16> projection{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        renderer.draw(context.Get(), quads, state, camera, projection);
        renderer.draw(context.Get(), {}, state, camera, projection);
        renderer.draw(context.Get(), quads, state, camera, projection);
        context->CopyResource(readback.Get(), output.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(SUCCEEDED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cannot read cloth pixels.");
        unsigned visible = 0;
        for (unsigned y = 8; y < 24; ++y) for (unsigned x = 8; x < 24; ++x) {
            const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
            if (pixel[0] || pixel[1] || pixel[2] || pixel[3]) ++visible;
        }
        context->Unmap(readback.Get(), 0);
        require(visible != 0, "Recovered cloth geometry and imported texture must reach the D3D target.");
    }

    class Services final : public playersheets::DrawServices, public playersheets::ShadowTextures {
    public:
        void matrices(const actormodel::Matrix& position, const actormodel::Matrix& normal) override {
            require(position == normal, "Original direct cloth uses the same scene position and normal matrix.");
            camera = position; events += 'm';
        }
        unsigned sceneLights(j3dmodel::MaterialState&) override { events += 'l'; return 5; }
        void specialLight(unsigned slot, j3dmodel::MaterialState&) override {
            require(slot == 7, "Original special cloth light uses slot seven."); events += 'h';
        }
        playersheets::DrawSlots beginShadow(j3dmodel::MaterialState& state) override {
            events += 's';
            return playersheets::beginShadow(shadow, state, *this);
        }
        void bind(unsigned slot) override {
            shadowSlots.push_back(slot);
        }
        void projectShadow(j3dmodel::MaterialState& state) override {
            events += 'p';
            playersheets::projectShadow(shadow, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, state);
        }
        unsigned roomColor(const playerscanner::Vector&) override { events += 'c'; return 0x12345678; }
        void texture(unsigned slot, const playermodels::GridTexture& texture) override {
            require(texture.primary && texture.wrapS == 0 && texture.minFilter == 1, "Direct draw retains original cloth texture sampling.");
            textureSlot = slot; events += 't';
        }
        void textureOffsets(unsigned coordinate, bool s, bool t) override {
            require(s && t, "Original cloth enables both texture offsets."); coordinateSlot = coordinate; events += 'o';
        }
        void begin(const j3dmodel::MaterialState& state, bool textured) override { captured = state; hasTexture = textured; events += 'b'; }
        void quad(const std::array<playersheets::DrawVertex, 4>& vertices) override { quads.push_back(vertices); }
        void end() override { events += 'e'; }
        void selectBuffers(playermodels::DrawBuffer& first, playermodels::DrawBuffer& second) override {
            firstBuffer = &first; secondBuffer = &second; events += 'd';
        }
        void updateModel(j3dmodel::ModelInstance& model) override { selectedModel = &model; events += 'u'; }
        void viewModel(j3dmodel::ModelInstance& model) override { selectedModel = &model; events += 'v'; }
        std::string events;
        j3dmodel::MaterialState captured;
        actormodel::Matrix camera{};
        std::vector<std::array<playersheets::DrawVertex, 4>> quads;
        playermodels::DrawBuffer* firstBuffer = nullptr;
        playermodels::DrawBuffer* secondBuffer = nullptr;
        j3dmodel::ModelInstance* selectedModel = nullptr;
        unsigned textureSlot = 0, coordinateSlot = 0;
        bool hasTexture = false;
        playersheets::ShadowState shadow{};
        std::vector<unsigned> shadowSlots;
    };
}

void checkSheetDraw(gameplay::ResourceOwner& resources) {
    {
        Services services;
        j3dmodel::MaterialState state;
        const actormodel::Matrix projection{2, 0, 0, 4, 0, 3, 0, 5, 0, 0, 4, 6};
        const actormodel::Matrix model{0, -1, 0, 10, 1, 0, 0, 20, 0, 0, 1, 30};
        services.shadow.color = {10, 20, 30, 40};
        services.shadow.postMatrices = {projection, model};
        services.shadow.projections = {projection, model};
        state.combiner.stages[7].colorSelection = 17;
        const auto slots = services.beginShadow(state);
        require(slots.stage == 2 && slots.texture == 2 && slots.coordinate == 2 &&
            services.shadowSlots == std::vector<unsigned>{0, 1}, "Shadow setup binds both original texture slots in order.");
        require(state.postMatrixRows[54] == std::array<float, 4>{2, 0, 0, 4} &&
            state.postMatrixRows[59] == std::array<float, 4>{0, 0, 1, 30} &&
            state.coordinates[0].postMatrix == 118 && state.coordinates[1].postMatrix == 121,
            "Shadow setup retains both complete post-projection matrices.");
        require(state.combiner.constants[3] == services.shadow.color &&
            state.combiner.swaps[1] == std::array<int, 4>{0, 3, 2, 3} &&
            state.combiner.stages[1].colorOperation == std::array<int, 5>{10, 0, 0, 1, 0} &&
            state.combiner.stages[1].alphaOperation == std::array<int, 5>{10, 0, 0, 1, 1} &&
            state.combiner.stages[7].colorSelection == 17,
            "Shadow setup preserves compare operations, register destinations and unrelated stage state.");
        playersheets::projectShadow(services.shadow, model, state);
        require(state.matrixRows[30] == std::array<float, 4>{0, -2, 0, 24} &&
            state.matrixRows[31] == std::array<float, 4>{3, 0, 0, 65} &&
            state.matrixRows[32] == std::array<float, 4>{0, 0, 4, 126} &&
            state.matrixRows[33] == std::array<float, 4>{-1, 0, 0, -10} &&
            state.coordinates[0].postMatrix == 125 && state.coordinates[1].info[2] == 33,
            "Shadow projection concatenates in original order and replaces the temporary post-matrix generators.");
        std::vector<j3dmodel::MaterialLight> lights(3);
        require(playersheets::loadSceneLights(lights, state) == 7 && state.lights[2].has_value(),
            "Scene light upload returns the mask of the current original light count.");
        require(playersheets::loadSceneLights({}, state) == 0 && state.lights[2].has_value(),
            "Empty scene light upload leaves previously loaded slots intact.");
        bool rejected = false;
        try { playersheets::loadSceneLights(std::vector<j3dmodel::MaterialLight>(9), state); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Native scene light upload rejects an invalid slot count.");
    }
    auto& shared = *resources.managers().players().models;
    playermodels::GridModels models(shared, 2, 2);
    playersheets::Layout layout;
    layout.width = layout.height = 2;
    layout.border = {0, 0, 1, 1}; layout.drops = {};
    layout.corners = {{{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}}};
    layout.secondaryTexture = 13;
    playersheets::Frame frame(layout, models, shared, resources.executable());
    j3dmodel::MaterialState state;
    state.pixels.alpha = {4, 120, 2, 1, 90};
    const actormodel::Matrix camera{1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30};
    const std::array<std::uint8_t, 4> ambient{10, 20, 30, 40};
    Services services;
    frame.grid.points()[3].active = false;
    playersheets::drawFrame(frame, models, shared, 1, camera, ambient, state, services);
    require(services.events == "mltobe" && services.quads.size() == 1 && services.camera == camera,
        "Original cloth draw emits a quad even when only its unchecked fourth grid corner is inactive.");
    const unsigned indices[]{0, 1, 3, 2};
    for (unsigned i = 0; i < 4; ++i) require(services.quads[0][i].position == frame.grid.points()[indices[i]].position,
        "Cloth quad winding follows the original FIFO vertex order.");
    require(services.captured.pixels.depth == std::array<std::uint8_t, 3>{1, 1, 1} &&
        services.captured.pixels.blend == std::array<std::uint8_t, 4>{1, 4, 5, 15} &&
        services.captured.pixels.alpha == state.pixels.alpha && services.captured.pixels.depthLocation == 0 && state.pixels.depthLocation == 1,
        "Cloth preserves inherited alpha testing and restores the original depth-comparison location.");
    require(services.captured.channels[0].lights == 5 && services.captured.channels[2].lights == 5 &&
        services.captured.ambientColors[0] == ambient && services.captured.cullMode == 0,
        "Cloth draw binds original scene lighting, ambient color and no culling.");
    renderCloth(services.quads, services.captured, models.texture, 0);
    services = Services{};
    frame.grid.points()[1].active = false;
    playersheets::drawFrame(frame, models, shared, 1, camera, ambient, state, services);
    require(services.quads.empty(), "An inactive checked corner suppresses its quad.");
    services = Services{};
    models.texture.primary.reset();
    playersheets::drawFrame(frame, models, shared, 1, camera, ambient, state, services);
    require(!services.hasTexture && services.events == "mlbe" && services.captured.textureCoordinates == 0 &&
        services.captured.combiner.stages[0].colorInputs == std::array<int, 4>{15, 15, 15, 10},
        "Untextured cloth uses the original raster-color combiner without binding a texture.");

    layout.anchored = false;
    layout.reversePlanes = true;
    playersheets::Frame shadow(layout, models, shared, resources.executable());
    services = Services{};
    services.shadow.color = {255, 255, 255, 255};
    for (auto& matrix : services.shadow.projections) matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    playersheets::drawFrame(shadow, models, shared, 1, camera, ambient, state, services);
    require(services.events.empty(), "Unanchored direct cloth is excluded from the first draw phase.");
    playersheets::drawFrame(shadow, models, shared, 2, camera, ambient, state, services);
    require(services.events == "mlhstocpbe" && services.textureSlot == 2 && services.coordinateSlot == 2 &&
        services.captured.combiner.count == 6 && services.captured.textureCoordinates == 4,
        "Special cloth pass preserves shadow setup, reversed-plane alpha and appended stage order.");
    require(services.captured.combiner.colors[1][3] == 0x78 && services.captured.colorChannels == 2 &&
        services.captured.channels[3].lights == 0x80, "Special cloth alpha uses the room color and original light channel.");
    require(services.shadowSlots == std::vector<unsigned>{0, 1} &&
        services.captured.combiner.stages[1].colorSelection == 31 &&
        services.captured.combiner.stages[1].alphaOperation[4] == 1 &&
        services.captured.coordinates[0].info[2] == 30,
        "Cloth special pass executes the recovered shadow setup and projection before submission.");
    renderCloth(services.quads, services.captured, models.texture, 2);
    require(state.combiner.stages[0].swap == std::array<int, 2>{0, 0} && state.combiner.stages[0].colorSelection == 12 &&
        state.combiner.stages[0].order == std::array<int, 3>{255, 255, 255}, "Special cloth draw restores the original shared combiner state.");

    layout.mode = 1;
    layout.anchored = true;
    playersheets::Frame model(layout, models, shared, resources.executable());
    services = Services{};
    playersheets::drawFrame(model, models, shared, 0, camera, ambient, state, services);
    require(services.events == "du" && services.firstBuffer == &shared.buffer(2) && services.secondBuffer == &shared.buffer(3) &&
        services.selectedModel == models.instances[0].get(), "Model phase uses the original shared draw buffers and retained model instance.");
    services = Services{};
    model.motion.detached = true;
    playersheets::drawFrame(model, models, shared, 0, camera, ambient, state, services);
    require(services.events.empty(), "Detached sheets stop model-update submission.");
    playersheets::drawFrame(model, models, shared, 1, camera, ambient, state, services);
    require(services.events.back() == 'v', "Original view calculation still follows detached cloth geometry submission.");
    model.suction.completed = true;
    services = Services{};
    for (int phase : {-1, 0, 1, 2, 3}) playersheets::drawFrame(model, models, shared, phase, camera, ambient, state, services);
    require(services.events.empty(), "Completed sheets skip every draw phase.");
    for (auto& instance : models.instances) instance->model->geometry.textureReplacements.clear();
}
