#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/world/mapcollisionstate.h"
#include <functional>

namespace ghostmotion {
    float vertical(const actormodel::InitialPose& pose, float amplitude, float maximum,
        float height, float floor, float ceiling, float targetHeight, unsigned actor, std::uint32_t frame, unsigned phaseStep = 0x400);
    std::array<float, 3> horizontal(const actormodel::InitialPose& pose, const std::array<float, 3>& velocity,
        float acceleration, float maximum, std::uint16_t angle);
}

namespace enemystrategy {
    struct RandomMovementServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const jmp::ToolDataRef& source;
        const mapcollision::State& collision;
        const actormodel::InitialPose& pose;
        std::function<std::uint32_t()> random;
        std::function<std::uint32_t()> frame;
    };

    class RandomMovement final : public Strategy {
    public:
        explicit RandomMovement(RandomMovementServices services);
        void initializeBehavior() override;
        void behavior() override;

    private:
        RandomMovementServices services;
        bool contactEnabled = true;
        int polygon = -1;
        float floatingHeight = 0;
        void move(float height);
    };
}
