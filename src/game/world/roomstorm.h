#pragma once

#include "game/events/missionresources.h"
#include "game/world/roomlightpool.h"
#include "game/world/roomlocation.h"
#include "game/actors/actorinitialpose.h"
#include "game/scene/gameplaycameraowner.h"
#include <functional>

namespace roomlighting {
    class Storm {
    public:
        Storm(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
            const actormodel::InitialPose& pose);
        void update(unsigned room, const roomlocation::Room& bounds, unsigned roomFlags,
            const gameplaycamera::CameraOwner& camera, float playerHeight, Pool& lights,
            const std::function<std::uint32_t()>& random, const std::function<void()>& thunder);
        bool active() const;
        unsigned strikes() const;
        unsigned setting(unsigned room) const;

    private:
        struct Key {
            std::array<std::uint16_t, 6> values;
            std::uint8_t interpolate, duration;
        };
        const actormodel::InitialPose& pose;
        std::array<std::uint8_t, 256> settings{};
        std::array<std::array<std::uint16_t, 2>, 5> angles{};
        std::array<Key, 7> keys;
        std::array<std::uint16_t, 2> direction{};
        float distance, height, anchorHeight, randomDivisor;
        float roomMiddle, roomHeight, roomDepth;
        double threshold;
        unsigned strikeCount = 0, key = 0, tick = 0, ownerTicks = 0;
        bool playing = false, ownerActive = false;
    };
}
