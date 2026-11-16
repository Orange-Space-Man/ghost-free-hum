#pragma once

#include "game/player/playeractor.h"
#include "game/player/playereventpayload.h"
#include "game/actors/actorcollisionowner.h"

namespace playerevents {
    class ScareResponse {
    public:
        ScareResponse(const std::vector<std::uint8_t>& executable, const actormodel::Catalog& models);
        unsigned run(playeractor::Player& player, const Payload& payload, const actors::Registry& actors) const;
        static bool visible(const gameplaycamera::CameraOwner& camera, const std::array<float, 3>& position);

    private:
        actorcollision::Catalog collisions;
        actormodel::InitialPose pose;
        float maximumDistance;
        bool visible(const gameplaycamera::CameraOwner& camera, const actors::MoveActor& actor) const;
    };
}
