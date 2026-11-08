#pragma once

#include "game/models/modellighting.h"
#include "jlib/j3d/j3dmaterialstate.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace modeleffects {
    class Lighting {
    public:
        explicit Lighting(const std::vector<std::uint8_t>& executable);
        unsigned prepare(j3dmodel::MaterialState& state, const j3dmodel::Matrix& view,
            const std::vector<modellighting::RoomLight>& lights, const originalmath::FloatTrig& trig) const;

    private:
        j3dmodel::MaterialLight fixed, specular;
        std::array<std::array<std::uint8_t, 4>, 2> ambient{};
        std::array<float, 3> direction{};
        float specularDistance = 0;
    };
}
