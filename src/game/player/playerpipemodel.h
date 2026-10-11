#pragma once

#include "game/player/playervacuummodel.h"

namespace playermodels {
    struct PipeParameters : PathParameters {
        std::uint32_t field3C = 0, field40 = 0;
        float field44 = 0;
        std::uint32_t field48 = 0;
        float field4C = 1;
    };

    struct PipeSection {
        std::uint8_t state = 0;
        std::array<float, 3> field4;
    };

    class PipeModel : public VacuumPath {
    public:
        PipeModel(const SharedModels& shared, unsigned count);
        ~PipeModel() override;
        std::int32_t pulse(double speed, double amount);
        std::array<PipeSection, 4> sections;
        std::unique_ptr<float[]> values;
        std::unique_ptr<j3dmodel::ModelInstance> instance;
        std::uint16_t joint;
        std::uint32_t field64;
        std::array<std::array<float, 3>, 4> positions{};
    };
}
