#include "playersheetdraw.h"

#include <stdexcept>

namespace {
    void appendShadow(j3dmodel::MaterialState& state, unsigned& count) {
        auto& first = state.combiner.stages.at(count++);
        first.order = {255, 255, 5};
        first.colorInputs = {15, 15, 15, 0}; first.alphaInputs = {7, 5, 1, 7};
        first.colorOperation = first.alphaOperation = {0, 0, 0, 1, 2};
        auto& second = state.combiner.stages.at(count++);
        second.colorSelection = 15; second.alphaSelection = 28;
        second.order = {255, 255, 255};
        second.colorInputs = {0, 14, 5, 15}; second.alphaInputs = {7, 7, 7, 0};
        second.colorOperation = second.alphaOperation = {0, 0, 0, 1, 0};
    }
    void restoreShadow(j3dmodel::MaterialState& state) {
        for (unsigned i : {0U, 1U, 2U, 4U}) state.combiner.stages[i].swap = {0, 0};
        for (unsigned i = 0; i < 6; ++i) state.combiner.stages[i].colorSelection = 12;
        for (unsigned i = 0; i < 4; ++i) state.combiner.stages[i].alphaSelection = 28;
        state.combiner.stages[0].order = state.combiner.stages[1].order = {255, 255, 255};
    }
}

void playersheets::drawGrid(const Frame& frame, const playermodels::GridModels& models, bool shadow,
    const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
    j3dmodel::MaterialState& state, DrawServices& services) {
    services.matrices(camera, camera);
    state.cullMode = 0;
    const unsigned lights = services.sceneLights(state);
    state.channels[0] = state.channels[2] = {1, 0, 0, lights, 2, 1};
    state.materialColors[0] = {255, 255, 255, 255};
    state.ambientColors[0] = ambient;
    state.colorChannels = shadow ? 2 : 1;
    if (shadow) {
        services.specialLight(7, state);
        state.channels[3] = {1, 0, 0, 0x80, 2, 1};
        state.ambientColors[1] = {};
    }
    const DrawSlots slots = shadow ? services.beginShadow(state) : DrawSlots{};
    const bool textured = bool(models.texture.primary);
    unsigned count = slots.stage + 1, coordinates = slots.coordinate;
    auto& stage = state.combiner.stages.at(slots.stage);
    if (textured) {
        services.texture(slots.texture, models.texture);
        state.coordinates.at(slots.coordinate) = {{1, 4, 60}, 0, 125};
        services.textureOffsets(slots.coordinate, true, true);
        stage.order = {int(slots.coordinate), int(slots.texture), 4};
        stage.colorInputs = {15, 10, 8, 15}; stage.alphaInputs = {7, 5, 4, 7};
        ++coordinates;
    } else {
        stage.order = {255, 255, 4};
        stage.colorInputs = {15, 15, 15, 10}; stage.alphaInputs = {7, 7, 7, 5};
    }
    stage.colorOperation = stage.alphaOperation = {0, 0, 0, 1, 0};
    stage.swap = {0, 0};
    if (textured && frame.grid.layout().reversePlanes) {
        auto& alpha = state.combiner.stages.at(count++);
        alpha.order = {255, 255, 255};
        alpha.colorInputs = {15, 15, 15, 0}; alpha.alphaInputs = {7, 0, 1, 7};
        alpha.colorOperation = alpha.alphaOperation = {0, 0, 0, 1, 0};
        alpha.swap = {0, 0};
        const auto& matrix = frame.grid.matrix();
        state.combiner.colors[1] = {0, 0, 0, int(services.roomColor({matrix[3], matrix[7], matrix[11]}) & 255)};
        ++coordinates;
    }
    if (shadow) {
        services.projectShadow(state);
        appendShadow(state, count);
    }
    state.textureCoordinates = coordinates;
    state.combiner.count = count & 255;
    state.pixels.blend = {1, 4, 5, 15};
    state.pixels.depth = {1, 1, 1};
    state.pixels.depthLocation = 0;
    services.begin(state, textured);
    const auto& layout = frame.grid.layout();
    for (unsigned x = 0; x + 1 < layout.width; ++x) {
        for (unsigned y = 0; y + 1 < layout.height; ++y) {
            const unsigned first = y * layout.width + x;
            const std::array<unsigned, 4> indices{first, first + 1, first + layout.width + 1, first + layout.width};
            const auto& points = frame.grid.points();
            if (!points[indices[0]].active || !points[indices[1]].active || !points[indices[3]].active) continue;
            std::array<DrawVertex, 4> vertices;
            for (unsigned i = 0; i < 4; ++i) {
                const auto& point = points[indices[i]];
                vertices[i] = {point.position, point.normal, point.texture};
            }
            services.quad(vertices);
        }
    }
    services.end();
    if (shadow) restoreShadow(state);
    state.pixels.depthLocation = 1;
}

void playersheets::drawFrame(const Frame& frame, playermodels::GridModels& models, playermodels::SharedModels& shared,
    int phase, const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
    j3dmodel::MaterialState& state, DrawServices& services) {
    if (frame.suction.completed) return;
    const unsigned mode = frame.grid.layout().mode;
    if (mode > 3) throw std::runtime_error("Invalid original cloth draw model mode.");
    if (phase == 0) {
        if (!frame.motion.detached) {
            services.selectBuffers(shared.buffer(2), shared.buffer(3));
            if (mode) services.updateModel(*models.instances[mode - 1]);
        }
    } else if (phase == 1 || phase == 2) {
        if (phase == 2 || frame.grid.layout().anchored)
            drawGrid(frame, models, phase == 2 && !frame.grid.layout().anchored, camera, ambient, state, services);
        if (mode) services.viewModel(*models.instances[mode - 1]);
    }
}

void playersheets::drawOwner(Owner& owner, playermodels::GridOwner& models, playermodels::SharedModels& shared,
    int phase, const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
    j3dmodel::MaterialState& state, DrawServices& services) {
    for (unsigned i = 0; i < models.grids.size(); ++i) if (models.fields16BC[i])
        drawFrame(owner.sheet(i), *models.grids[i], shared, phase, camera, ambient, state, services);
}
