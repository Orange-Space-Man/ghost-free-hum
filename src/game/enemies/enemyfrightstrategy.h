#pragma once

#include "game/enemies/enemyappearancestrategy.h"

namespace enemystrategy {
    struct FrightServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const actormodel::InitialPose& pose;
        std::function<std::array<float, 3>(unsigned)> playerPosition;
        std::function<void()> beginAttack;
        std::function<void(const std::array<float, 3>&)> scaleAttack;
        std::function<bool(unsigned)> frightenPlayer;
        std::function<void(std::array<float, 3>&)> projectFloor;
        std::function<void(float, float, const std::array<float, 3>&, const std::array<std::uint8_t, 4>&)> drawAttack;
        std::function<int(bool)> recoveryDuration;
    };

    class Fright final : public Strategy {
    public:
        explicit Fright(FrightServices services, float rate);
        void initializeBehavior() override;
        void behavior() override;
        bool attacking() const;
        bool succeeded() const;

    private:
        FrightServices services;
        float rate;
        bool success = false;
        void animation(const char* name, unsigned duration);
        std::array<float, 3> facePlayer();
        void attack();
    };
}
