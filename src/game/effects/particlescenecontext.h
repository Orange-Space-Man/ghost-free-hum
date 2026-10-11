#pragma once

#include "game/effects/particlesuction.h"
#include "game/actors/actorinitialpose.h"
#include "game/world/roomtraversalstate.h"
#include "game/enemies/enemymanager.h"

namespace particles {
    struct SceneContext {
        const std::array<float, 3>* actorPosition = nullptr;
        std::array<float, 3> orientation{};
        float angle = 0, height = 0, magnitude = 0;
        SuctionContext suction{};
        float control = 0;
        std::array<float, 3> endpoint{};
        const std::array<float, 3>* field38 = nullptr;
        const std::array<float, 3>* field3C = nullptr;
        float distance = 0, field44 = 0, field48 = 0, field4C = 0;
        unsigned room = 0;
        float field54 = 0;
        bool visible = false;
        std::uint8_t field59 = 0, field5A = 0, field5B = 0, field5C = 0;
        void refreshPlayer(double heightOffset, const playeractor::View& player, const actormodel::InitialPose& pose,
            enemies::Manager& manager, const roomtraversal::State& visibility);
    };

    struct SecondarySource {
        bool active;
        const std::array<float, 3>* position;
        float field14;
        std::array<float, 3> velocity, direction;
    };

    void refreshSecondary(SceneContext& context, double heightOffset, const SecondarySource* source);

    struct SceneContexts {
        SceneContext player, secondary;
        void refresh(const playeractor::View& view, const actormodel::InitialPose& pose, enemies::Manager& manager,
            const roomtraversal::State& visibility, const SecondarySource* source);
    };
}
