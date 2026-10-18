#pragma once

#include "moneyscatterpool.h"

namespace moneyscatter {
    void updateModel(Particle& particle);

    class Models {
    public:
        Models(Pool& pool, const std::shared_ptr<j3dmodel::ModelData>& coin, float retainedAuxiliarySpin);
        void updateAuxiliary(const Vector& cameraPosition, const Vector& cameraDirection);
        Particle auxiliary;
    };
}
