#pragma once

#include "moneyscatterpool.h"

namespace moneyscatter {
    struct RetainedState {
        float auxiliarySpin = 0;
    };

    void updateModel(Particle& particle);

    class Models {
    public:
        Models(Pool& pool, const std::shared_ptr<j3dmodel::ModelData>& coin, float retainedAuxiliarySpin);
        Models(Pool& pool, const std::shared_ptr<j3dmodel::ModelData>& coin, RetainedState& state);
        void updateAuxiliary(const Vector& cameraPosition, const Vector& cameraDirection);
        Particle auxiliary;

    private:
        RetainedState* retained = nullptr;
    };
}
