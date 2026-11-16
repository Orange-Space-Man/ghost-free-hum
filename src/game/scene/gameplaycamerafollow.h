#pragma once

#include "game/scene/gameplaycameraowner.h"
#include "game/player/playeractorview.h"
#include <vector>

namespace gameplaycamera {
    class Follow {
    public:
        explicit Follow(const std::vector<std::uint8_t>& executable, unsigned map = 2);
        void update(CameraOwner& camera, const playeractor::View& player, unsigned room, unsigned playerIndex = 0);
        unsigned preset() const;
        static constexpr unsigned captureCallback = 0x80022bc4;
        void beginTarget(CameraOwner& camera) const;
        void updateTarget(CameraOwner& camera, const std::array<float, 3>& player,
            const std::array<float, 3>& target, unsigned room) const;

    private:
        struct Room {
            unsigned id, flags;
            std::array<float, 3> minimum, extent;
        };
        std::vector<Room> rooms;
        void select(unsigned value, unsigned delay);
        void refreshRoom(unsigned room);
        void updateCapture(CameraOwner& camera, const playeractor::View& player, unsigned room, unsigned playerIndex);
        std::array<float, 3> aimFor(const std::array<float, 3>& subject, float lead, bool inverted) const;
        std::array<float, 3> eyeFor(const std::array<float, 3>& subject, float lead, float cameraTargetY, float height, float depth) const;
        void approach(std::array<float, 3>& value, const std::array<float, 3>& target, float rate) const;
        std::array<std::array<float, 4>, 16> presets;
        std::array<float, 3> minimum{}, extent{};
        std::array<float, 2> leadX{}, leadZ{};
        std::array<float, 4> parameters;
        unsigned roomId, roomFlags, selected = 0, selectionTicks = 0;
        unsigned nearDelay, normalDelay;
        float leadScale, leadDecay, inset, heightThreshold, followRate, absentRoomRate;
        float nearThreshold, farThreshold, reactionHeight, reactionDepth;
        float quantization, inverseQuantization, smoothingScale, fieldOfView, coincidentOffset;
        float nearPlane, farPlane;
        float targetHeight, targetMinimumDepth, targetRate;
        float captureFarDistance, captureLookDistance, eyeHeight = 0, eyeDepth = 0;
        std::array<float, 3> up;
    };
}
