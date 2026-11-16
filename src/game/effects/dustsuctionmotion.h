#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace runningdust {
    struct SuctionParticle {
        std::array<float, 3> position{}, acceleration{}, velocity{};
        std::uint16_t pulse = 0;
        float rotationSpeed = 0, rotation = 0, size = 0, baseSize = 1;
    };

    struct SuctionParameters {
        float pulseAmplitude = 0, pulseStep = 0, accelerationScale = 0, velocityScale = 0;
    };

    class SuctionMotion {
    public:
        explicit SuctionMotion(const std::vector<std::uint8_t>& executable);
        void advance(float distance, float strength, const std::array<float, 3>& origin,
            const std::array<float, 3>& axis, const std::array<float, 3>& direction,
            const SuctionParameters& parameters, SuctionParticle& particle,
            const std::function<std::uint32_t()>& random) const;
        void outward(float distance, float strength, const std::array<float, 3>& direction,
            const SuctionParameters& parameters, SuctionParticle& particle,
            const std::function<std::array<float, 3>()>& randomDirection) const;

    private:
        void decay(float distance, SuctionParticle& particle) const;
        void finish(float strength, const SuctionParameters& parameters, SuctionParticle& particle) const;
        std::array<float, 4096> sine{};
        float nearDistance, minimumSize, nearDecay, farDecay, transverseThreshold, maximumSpeed;
        float one, rotationBase, randomDivisor;
        float outwardAcceleration, outwardVelocity, minimumOutwardVelocity;
    };
}
