#pragma once

#include "game/actors/actorkeysampler.h"

namespace actormodel {
    class InitialPose {
    public:
        explicit InitialPose(const std::vector<std::uint8_t>& executable);
        void initialize(Instance& instance) const;
        Matrix rotation(const std::array<std::int16_t, 3>& angles) const;
        std::uint16_t angle(double x, double y) const;
        std::array<float, 3> horizontalDirection(std::uint16_t angle) const;
        std::array<std::int16_t, 3> angles(const Matrix& matrix) const;
        Matrix jointMatrix(const actoranimation::JointPose& pose, bool scale) const;

    private:
        std::array<float, 4096> sine, cosine;
        std::array<std::uint16_t, 1025> arcTangent;
    };
}
