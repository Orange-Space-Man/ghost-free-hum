#pragma once

#include "playersheetframe.h"
#include "jlib/j3d/j3dmaterialstate.h"

namespace playersheets {
    struct DrawVertex {
        playerscanner::Vector position, normal;
        std::array<float, 2> texture;
    };

    struct DrawSlots {
        unsigned stage = 0, texture = 0, coordinate = 0;
    };

    class DrawServices {
    public:
        virtual ~DrawServices() = default;
        virtual void matrices(const actormodel::Matrix& position, const actormodel::Matrix& normal) = 0;
        virtual unsigned sceneLights(j3dmodel::MaterialState& state) = 0;
        virtual void specialLight(unsigned slot, j3dmodel::MaterialState& state) = 0;
        virtual DrawSlots beginShadow(j3dmodel::MaterialState& state) = 0;
        virtual void projectShadow(j3dmodel::MaterialState& state) = 0;
        virtual unsigned roomColor(const playerscanner::Vector& position) = 0;
        virtual void texture(unsigned slot, const playermodels::GridTexture& texture) = 0;
        virtual void textureOffsets(unsigned coordinate, bool s, bool t) = 0;
        virtual void begin(const j3dmodel::MaterialState& state, bool textured) = 0;
        virtual void quad(const std::array<DrawVertex, 4>& vertices) = 0;
        virtual void end() = 0;
        virtual void selectBuffers(playermodels::DrawBuffer& first, playermodels::DrawBuffer& second) = 0;
        virtual void updateModel(j3dmodel::ModelInstance& model) = 0;
        virtual void viewModel(j3dmodel::ModelInstance& model) = 0;
    };

    void drawGrid(const Frame& frame, const playermodels::GridModels& models, bool shadow,
        const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
        j3dmodel::MaterialState& state, DrawServices& services);
    void drawFrame(const Frame& frame, playermodels::GridModels& models, playermodels::SharedModels& shared, int phase,
        const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
        j3dmodel::MaterialState& state, DrawServices& services);
    void drawOwner(Owner& owner, playermodels::GridOwner& models, playermodels::SharedModels& shared, int phase,
        const actormodel::Matrix& camera, const std::array<std::uint8_t, 4>& ambient,
        j3dmodel::MaterialState& state, DrawServices& services);
}
