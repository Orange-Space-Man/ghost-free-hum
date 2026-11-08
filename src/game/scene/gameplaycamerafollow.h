#pragma once

#include "game/scene/gameplaycameraowner.h"
#include "game/player/playeractorview.h"
#include <vector>

namespace gameplaycamera {
    class Follow {
    public:
        explicit Follow(const std::vector<std::uint8_t>& executable);
        void update(CameraOwner& camera, const playeractor::View& player, unsigned room, unsigned playerIndex = 0);
        unsigned preset() const;

    private:
        struct Room {
            unsigned id, flags;
            std::array<float, 3> minimum, extent;
        };
        std::vector<Room> rooms;
        void select(unsigned value, unsigned delay);
        void approach(std::array<float, 3>& value, const std::array<float, 3>& target, float rate) const;
        std::array<std::array<float, 4>, 5> presets;
        std::array<float, 3> minimum{}, extent{};
        std::array<float, 2> leadX{}, leadZ{};
        std::array<float, 4> parameters;
        unsigned roomId, roomFlags, selected = 0, selectionTicks = 0;
        unsigned nearDelay, normalDelay;
        float leadScale, leadDecay, inset, heightThreshold, followRate, absentRoomRate;
        float nearThreshold, farThreshold, reactionHeight, reactionDepth;
        float quantization, inverseQuantization, smoothingScale, fieldOfView, coincidentOffset;
        float nearPlane, farPlane;
        std::array<float, 3> up;
    };
}
