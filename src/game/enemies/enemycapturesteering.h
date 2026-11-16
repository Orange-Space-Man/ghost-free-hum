#pragma once

#include "game/enemies/enemycapturemotion.h"
#include <cstdint>
#include <functional>

namespace ghostcapture {
    struct SteeringInput {
        Vector position{}, anchor{}, contactNormal{};
        bool contact = false, interrupted = false;
    };

    class Steering {
    public:
        explicit Steering(std::function<std::uint32_t()> random);
        void begin(Motion& motion, const SteeringInput& input);
        void update(Motion& motion, const SteeringInput& input);
        unsigned state = 0;
        float frames = 0, probability = 0, attempts = 0;
        bool disabled = false;

    private:
        std::function<std::uint32_t()> random;
        Vector impulse{};
        float fraction();
        void initialize(Motion& motion, const SteeringInput& input);
        Vector direction(const Motion& motion, const SteeringInput& input);
    };
}
