#pragma once

#include "game/player/playervelocitycontext.h"

namespace playermotion {
    struct PlatformMotion {
        Vector position{};
        float radius = 0;
        std::int16_t rotation = 0;
    };

    class VelocityFrame {
    public:
        VelocityFrame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose);
        void run(playeractor::Player& player, const mapcollision::State& collision, const std::array<PlatformMotion, 2>& platforms,
            const std::function<void(std::uint32_t, const Vector&)>& sound) const;
        void constrain(playeractor::Player& player) const;

    private:
        const actormodel::InitialPose& pose;
        VelocityContext context;
        playeractions::Context actions;
        float scriptedSpeed, scriptedBlend;
        float radialForce, radialTurnDistance;
    };
}
