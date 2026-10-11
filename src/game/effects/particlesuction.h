#pragma once

#include "game/player/playeractorview.h"
#include "game/effects/particlenodestate.h"

#include <functional>

namespace particles {
    struct SuctionContext {
        const std::array<float, 3>* position;
        const std::array<float, 3>* direction;
        float strength;
    };

    class Suction {
    public:
        Suction(const playeractor::View& player, const SuctionContext& context, std::function<std::uint32_t()> random);
        unsigned apply(float accelerationScale, float unused, float maximumSpeed, const std::array<float, 3>& position,
            std::array<float, 3>& velocity, std::array<float, 3>& acceleration, std::uint8_t& captured);
        bool retainCapture = true;
        float distance = 0;
        std::array<float, 3> direction{};

    private:
        const playeractor::View& player;
        const SuctionContext& context;
        std::function<std::uint32_t()> random;
        void force(float accelerationScale, float maximumSpeed, const std::array<float, 3>& position,
            std::array<float, 3>& velocity, std::array<float, 3>& acceleration);
    };

    struct SuctionCallback : NodeCallback {
        explicit SuctionCallback(Suction& suction);
        bool advance(Emitter& emitter, Node& node) override;
        bool advance(Node& node) const;
        float accelerationScale = 1000, field8 = 4, maximumSpeed = 18;
        std::uint8_t capture = 0;

    private:
        Suction& suction;
    };
}
