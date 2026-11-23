#pragma once

#include "game/enemies/enemyitemstrategy.h"
#include "game/actors/actortrailcontroller.h"

namespace enemystrategy {
    // EnBanana (800DF998), created by EnAtkBanaoba through the enemy manager.
    class Banana final : public Strategy, public characterevent::Observer {
    public:
        explicit Banana(ItemEnvironment& environment);
        void begin() override;
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t collideWithPlayer(characterevent::Event& event) override;

    private:
        ItemEnvironment& environment;
        ItemPhysics physics;
        actortrail::Controller trail;
        gameparameters::Parameters parameters;
        std::uint32_t remaining = 0, blink = 0;
        bool placed = false, grounded = false;
        actors::MoveActor& actor() const;
        void remove();
        void selectAnimation(const char* name);
    };
}
