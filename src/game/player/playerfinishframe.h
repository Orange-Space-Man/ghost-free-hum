#pragma once

#include "game/player/playeractor.h"
#include "game/player/playereventpayload.h"

namespace playeractor {
    struct FinishServices {
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        std::function<void(Player&, const playerevents::Payload&)> damage;
        std::function<void(bool)> interactionActive;
        std::function<void(Player&)> interactionEvent;
        std::function<void(actors::MoveActor&)> interactionOwner;
    };

    class FinishFrame {
    public:
        FinishFrame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose);
        float bodyScale(const State& state) const;
        void run(Player& player, const FinishServices& services) const;

    private:
        const actormodel::InitialPose& pose;
        std::array<float, 4> angleParameters;
        float cosineOffset, amplitude;
        std::int32_t concealDuration, powerDuration, recoveryDuration;
    };
}
