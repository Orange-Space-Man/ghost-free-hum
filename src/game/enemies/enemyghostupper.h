#pragma once

#include "game/enemies/enemyghostmovement.h"
#include "game/enemies/enemyghostparenthelpers.h"
#include "game/actors/characterevent.h"
#include "game/actors/actortrailcontroller.h"

namespace ghostparent {
    struct UpperServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const mapcollision::State& collision;
        const actormodel::InitialPose& pose;
        const actorcollision::Catalog& shapes;
        actortrail::Pool* trails;
        std::function<actors::MoveActor&(unsigned)> player;
        std::function<std::uint32_t()> frame;
        std::function<std::uint32_t()> random;
        std::uint32_t disappearFrames;
        std::function<void(unsigned)> damageNormal;
        std::function<std::uint32_t(unsigned)> damageStrike;
    };

    class UpperAttack final : public enemystrategy::Strategy, public characterevent::Observer {
    public:
        explicit UpperAttack(UpperServices services);
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t collideWithPlayer(characterevent::Event& event) override;

    private:
        UpperServices services;
        std::array<float, 3> target{};
        std::array<float, 3> heading{};
        bool struck = false;
        std::uint32_t elapsed = 0;
        actortrail::Controller trail;

        void animation(const char* name);
        std::array<float, 3> playerDelta() const;
        void approach(const std::array<float, 3>& delta);
        bool flight(float bend, float speed);
        void strike(unsigned player);
        bool touchingPlayer() const;
        bool disappear();
    };
}
