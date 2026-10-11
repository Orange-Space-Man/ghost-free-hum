#pragma once

#include "game/player/playercomponentstate.h"
#include "game/player/playercontrolstate.h"
#include "game/player/playerorientation.h"

namespace playerorientation {
    using Vector = std::array<float, 3>;

    struct Input {
        float blend = 0.5f, turnLimit = 0, acceleration = 0, damping = 0, maximumSpeed = 0;
        unsigned mode = 0;
        const playercontrols::Motion* movement = nullptr;
        const playercontrols::Motion* turning = nullptr;
        Vector position{}, velocity{}, targetPosition{}, targetDirection{};
        Quaternion target{};
        float threshold = 0;
        bool enabled = false, reverse = false, inverted = false;
    };

    Quaternion heading(const Vector& forward, bool inverted);
    Quaternion multiply(const Quaternion& first, const Quaternion& second);
    Quaternion interpolate(double weight, const Quaternion& first, const Quaternion& second);
    float update(playercomponents::QuaternionState& state, Quaternion& output, const Quaternion& source,
        const Input& input, const actormodel::InitialPose& pose);
}
