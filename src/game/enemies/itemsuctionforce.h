#pragma once

#include "game/effects/particlesuction.h"

namespace itemsuction {
    using Vector = std::array<float, 3>;

    struct Parameters {
        float braking = 0.73f;
        float durationScale = 2500;
        std::uint8_t durationBase = 0;
    };

    class Force {
    public:
        Force(const playeractor::View& player, const particles::SuctionContext& context);
        unsigned apply(double arrivalRadius, const Vector& position, Vector& velocity, Vector& acceleration,
            std::uint8_t& remaining, std::uint8_t& duration, bool countdown, const Parameters& parameters,
            const Vector& unwrittenDirection);
        float distance = 0;
        Vector direction{};

    private:
        const playeractor::View& player;
        const particles::SuctionContext& context;
        void accelerate(const Vector& position, Vector& velocity, Vector& acceleration);
    };
}
