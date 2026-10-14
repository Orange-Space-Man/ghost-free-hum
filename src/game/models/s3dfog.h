#pragma once

#include "game/models/s3denvironment.h"

namespace s3dfog {
    struct State {
        unsigned type = 0, magnitude = 0, shift = 0;
        float a = 0, c = 0;
        bool rangeEnabled = false;
        unsigned center = 0;
        std::array<unsigned, 10> range{};
        std::array<float, 4> color{};
    };

    State prepare(const s3denvironment::Fog& fog, const std::array<float, 16>& projection,
        float nearPlane, float farPlane);
}
