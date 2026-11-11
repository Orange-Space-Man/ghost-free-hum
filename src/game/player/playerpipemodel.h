#pragma once

#include "game/player/playervacuummodel.h"

namespace playermodels {
    struct PipeBoundary {
        std::array<float, 3> position{}, signs{};
        unsigned plane = 0;
        j3dmodel::Matrix matrix{};
    };

    struct PipeParameters : PathParameters {
        const std::array<float, 3>* frameDirection = nullptr;
        const std::array<float, 3>* startNormal = nullptr;
        float field44 = 0;
        const PipeBoundary* boundary = nullptr;
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
        void updatePulses();
        void publishJoints(double scale, const std::array<float, 3>& frameDirection);
        void updateMotion(const PipeParameters& parameters, const PathCollision& collision);
        std::array<PipeSection, 4> sections;
        std::unique_ptr<float[]> values;
        std::unique_ptr<j3dmodel::ModelInstance> instance;
        std::uint16_t joint;
        const std::array<float, 3>* startNormal = nullptr;
        float startFraction = 0;
        std::array<std::array<float, 3>, 4> positions{};

    private:
        bool hasBoundary = false;
        unsigned boundaryPlane = 0;
        void integrate(const PathParameters& parameters, const PathCollision& collision) override;
    };
}
