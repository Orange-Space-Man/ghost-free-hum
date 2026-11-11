#pragma once

#include "game/actors/actoreffectstate.h"
#include "game/player/playerweaponparameters.h"
#include "game/player/playerpipemodel.h"
#include <functional>

namespace actors { class MoveActor; }
namespace actormodel { class InitialPose; }

namespace playerweapons {
    class Owner {
    public:
        Owner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
            playermodels::PipeParameters& parameters);
        ~Owner();
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        void update(const std::vector<std::uint8_t>& executable, actors::MoveActor& player,
            actors::MoveActor* firstActor, actors::MoveActor* secondActor, unsigned action,
            unsigned attachedAnimation, unsigned attachedState, bool alternate, double shake,
            const actormodel::InitialPose& pose, const std::function<std::uint32_t()>& random);
        void updateEndpoints(const actors::MoveActor& firstActor, const actors::MoveActor& secondActor);
        void updatePipe(const playermodels::PipeParameters& globals, const actors::MoveActor& firstActor,
            const actors::MoveActor& secondActor, double scale, bool collisionEnabled, bool bodyConstraint,
            bool rigid, const playermodels::PathCollision& collision);
        std::int32_t actorType = -1, firstType = -1, secondType = -1;
        std::uint32_t fieldC;
        std::unique_ptr<Part> first, second;
        std::unique_ptr<playermodels::PipeModel> pipe;
        std::uint8_t field1C = 0;
        actoreffects::State effect;
    };
}
