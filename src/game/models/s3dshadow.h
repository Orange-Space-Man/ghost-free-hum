#pragma once

#include "game/models/s3denvironment.h"

namespace s3dshadow {
    struct Projection {
        std::array<float, 3> position{0, 200, 1000}, target{0, 200, -1000};
        std::array<float, 3> up{0, 1, 0};
        bool spotlight = false;
        float fieldOfView = 50, nearPlane = 1, farPlane = 1;
        std::array<float, 4> color{1, 1, 1, 1};
    };

    // 800093EC: actors stop receiving the projected shadow when the main camera's view direction and the shadow
    // projector's direction point apart (dot product below 0.17, constant 804D9920). 80060E70 evaluates it from the
    // shared camera block (main eye/target at +0/+C, projector eye/target at +24/+30) before enabling 8005DD50.
    bool receiveSuppressed(const s3denvironment::Camera& camera, const Projection& shadow);

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
