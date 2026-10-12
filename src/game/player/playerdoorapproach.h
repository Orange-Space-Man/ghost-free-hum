#pragma once

#include "game/player/playercomponentstate.h"
#include "game/player/playeractionstate.h"

namespace playerinteraction {
    using Vector = std::array<float, 3>;

    struct DoorGeometry {
        Vector position, normal, tangent;
    };

    struct DoorMotion {
        Vector displacement{}, facing{0, 0, 1};
    };

    enum class ApproachResult { Blocked = -1, Moving = 0, Arrived = 1 };

    class DoorApproach {
    public:
        explicit DoorApproach(const std::vector<std::uint8_t>& executable);
        void locate(playercomponents::DoorState& state, const Vector& position, const DoorGeometry& geometry) const;
        Vector facing(const Vector& position, const DoorGeometry& geometry) const;
        DoorMotion begin(playercomponents::DoorState& state, playeractions::Actions& actions,
            const Vector& position, std::uint32_t room, unsigned reaction, bool primary, std::int16_t pairedDoor) const;
        ApproachResult advance(playercomponents::DoorState& state, DoorMotion& motion,
            const Vector& position, float speed, bool contact) const;

    private:
        float epsilon, turnDistance, frontOffset, backOffset, lateralOffset, axisThreshold;
    };
}
