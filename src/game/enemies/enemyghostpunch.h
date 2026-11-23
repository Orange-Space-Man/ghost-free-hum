#pragma once

#include "game/enemies/enemyghostparenthelpers.h"
#include "game/enemies/enemyghostmovement.h"
#include "game/actors/characterevent.h"

namespace ghostparent {
    struct PunchServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const mapcollision::State& collision;
        const actormodel::InitialPose& pose;
        std::function<actors::MoveActor&(unsigned)> player;
        std::function<std::uint32_t()> frame;
        std::function<void(unsigned)> damageNormal;
        std::function<void(unsigned)> damagePunch;
        std::function<bool()> attackActive;
    };

    // EnAtkPunch, the default attack selected by mAttackPattern1=0.
    class PunchAttack final : public enemystrategy::Strategy, public characterevent::Observer {
    public:
        explicit PunchAttack(PunchServices services);
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t collideWithPlayer(characterevent::Event& event) override;

    private:
        PunchServices services;
        unsigned cycle = 0;
        bool touched = false;
        void animation(const char* name, unsigned duration);
        std::array<float, 3> playerDelta() const;
        void follow(const std::array<float, 3>& delta, float speedScale);
    };
}
