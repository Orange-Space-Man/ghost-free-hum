#pragma once

#include "game/models/s3denvironment.h"

namespace s3dshadow {
    struct Projection {
        std::array<float, 3> position{0, 200, 1000}, target{0, 200, -1000};
        float fieldOfView = 50, nearPlane = 1, farPlane = 1;
        std::array<float, 4> color{1, 1, 1, 1};
    };

    class Animation {
    public:
        bool read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error);
        Projection sample(float frame) const;

    private:
        Projection base;
        std::array<s3denvironment::Curve, 10> channels;
        std::array<int, 4> color{255, 255, 255, 255};
        bool enabled = false;
    };
}
