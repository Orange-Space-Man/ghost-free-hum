#pragma once

#include "game/scene/gameplaycameraprojection.h"
#include "game/world/roomlocation.h"
#include "game/world/roomvisibilitywalk.h"

namespace roommirror {
    class Owner {
    public:
        Owner(const std::vector<std::uint8_t>& executable, const roomlocation::Rooms& rooms);
        bool visible(const roomtraversal::State& visibility) const;
        gameplaycamera::DrawState camera(const s3denvironment::Camera& main);
        const actormodel::Matrix& matrix() const;
        unsigned room() const;
        unsigned width() const;
        unsigned height() const;

    private:
        actormodel::InitialPose pose;
        gameplaycamera::Projection projection;
        gameplaycamera::CameraOwner retained;
        actormodel::Matrix world;
        std::array<float, 3> position, normal;
        unsigned roomId;
        float distance;
    };
}
