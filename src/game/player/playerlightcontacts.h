#pragma once

#include "game/player/playeractor.h"
#include "game/world/roomvisibilitywalk.h"

namespace playeractor {
    class LightContacts {
    public:
        LightContacts(const std::vector<std::uint8_t>& executable, const actormodel::Catalog& models);
        bool contains(const playermodels::LightModel& light, const std::array<float, 3>& position, double radius,
            const gameplaycamera::CameraOwner* camera) const;
        void update(const playermodels::LightModel& light, enemies::Manager& manager,
            const actormodel::Catalog& models, const roomtraversal::State& visibility) const;

    private:
        actorcollision::Catalog collisions;
        actormodel::InitialPose pose;
        std::int32_t excludedCategory;
        unsigned message;
        float padding, nearRadius, epsilon;
        bool visible(const gameplaycamera::CameraOwner& camera, const std::array<float, 3>& position) const;
    };
}
