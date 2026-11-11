#pragma once

#include "game/models/s3dshadow.h"
#include "game/actors/actorinitialpose.h"
#include "game/scene/gameplaycameraowner.h"
#include "game/world/roomlocation.h"
#include "game/player/playeractorview.h"

namespace roomshadow {
    class Projector {
    public:
        explicit Projector(const std::vector<std::uint8_t>& executable);
        void update(const roomlocation::Room& room, unsigned flags, const gameplaycamera::CameraOwner& camera,
            float playerHeight, const actormodel::InitialPose& pose);
        void selectPlayerLight(const playeractor::View& view);
        s3dshadow::Projection projection(const playeractor::View& view, const gameplaycamera::CameraOwner& camera) const;
        void strike(unsigned preset, const actormodel::InitialPose& pose);
        const s3dshadow::Projection& projection() const;

    private:
        struct Preset {
            std::array<std::uint16_t, 2> angles;
            float nearPlane, farPlane, fieldOfView;
            int distance;
        };
        Preset ordinary, selected;
        std::array<Preset, 5> storms;
        s3dshadow::Projection value;
        float middle, height, depth, anchorHeight;
        unsigned ticks = 0;
        int opacity = 0;
        bool recovering = false, stormPriority = false, flashlight = false, enabled = true;
        float flashlightNear, flashlightFar;
        std::array<std::uint16_t, 2> frameAngles{};
        void publish(const actormodel::InitialPose& pose);
    };
}
