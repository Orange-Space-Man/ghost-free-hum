#pragma once

#include "playersheetdraw.h"

namespace playersheets {
    struct ShadowState {
        std::array<int, 4> color;
        std::array<actormodel::Matrix, 2> postMatrices;
        std::array<actormodel::Matrix, 2> projections;
    };

    class ShadowTextures {
    public:
        virtual ~ShadowTextures() = default;
        virtual void bind(unsigned slot) = 0;
    };

    DrawSlots beginShadow(const ShadowState& shadow, j3dmodel::MaterialState& state, ShadowTextures& textures);
    void projectShadow(const ShadowState& shadow, const actormodel::Matrix& model, j3dmodel::MaterialState& state);
    unsigned loadSceneLights(const std::vector<j3dmodel::MaterialLight>& lights, j3dmodel::MaterialState& state);
}
