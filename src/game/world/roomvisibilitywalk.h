#pragma once

#include "game/world/roomportalbounds.h"
#include "game/world/doordescriptors.h"

#include <functional>

namespace roomtraversal {
    struct SceneRoom {
        std::uint32_t id = 0, attributes = 0;
        std::vector<std::int16_t> portals;
    };

    struct Submission {
        std::uint32_t room = 0;
        std::int8_t marker = -1;
        Bounds bounds{};
    };

    struct WalkServices {
        std::function<bool(std::uint32_t)> doorBlocked;
        std::function<bool(const std::array<std::int32_t, 3>&)> centerVisible;
    };

    class Walk {
    public:
        Walk(const std::vector<SceneRoom>& rooms, const doordata::Table& portals, State& visibility,
            std::vector<std::uint32_t>& flags, std::vector<std::uint16_t>& savedFlags, unsigned map,
            const WalkServices& services);
        std::vector<Submission> stationary(unsigned room, unsigned buffer, const roomportal::Matrix& projection,
            const Bounds& viewport, std::uint8_t clippingMode, std::uint8_t floorOpacity);

    private:
        const std::vector<SceneRoom>& rooms;
        const doordata::Table& portals;
        State& visibility;
        std::vector<std::uint32_t>& flags;
        std::vector<std::uint16_t>& savedFlags;
        unsigned map;
        WalkServices services;
    };
}
