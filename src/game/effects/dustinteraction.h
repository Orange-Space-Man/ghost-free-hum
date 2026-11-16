#pragma once

#include "game/effects/dustsuctionmotion.h"

namespace runningdust {
    enum class ForceMode { None, Suction, Outward };
    enum class ForceResult { Ordinary, Forced, Removed };

    struct InteractionParticle : SuctionParticle {
        std::int16_t lifetime = 0;
        std::uint8_t influenced = 0;
        std::uint32_t forced = 0;
    };

    struct InteractionEmitter {
        unsigned type = 1, level = 0;
        std::int16_t captureLifetime = 0, releasedLifetime = 0;
        SuctionParameters motion;
    };

    struct ForceFrame {
        ForceMode mode = ForceMode::None;
        float strength = 0, radius = 0, cosine = 0;
        std::array<float, 3> origin{}, axis{};
        std::function<std::uint32_t()> random;
        std::function<std::array<float, 3>()> randomDirection;
        std::function<void(unsigned, unsigned)> capture;
    };

    ForceMode selectForce(bool overrideSuction, bool emitterOutward, bool vacuumPresent, int playerMode);

    class Interaction {
    public:
        explicit Interaction(const std::vector<std::uint8_t>& executable);
        ForceResult advance(const ForceFrame& frame, const InteractionEmitter& emitter, InteractionParticle& particle) const;

    private:
        SuctionMotion motion;
        float captureRadius, outwardNearRadius;
    };
}
