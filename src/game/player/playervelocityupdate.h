#pragma once

#include "game/world/mapcollisionstate.h"
#include "game/player/playercontrolstate.h"

#include <functional>

namespace playermotion {
    using Vector = std::array<float, 3>;

    struct VelocityInput {
        const playercontrols::Motion* motion = nullptr;
        const Vector* direction = nullptr;
        Vector position{};
        std::uint16_t floor = 0xffff, wall = 0xffff, probe = 0xffff;
        float drag = 1, idleFriction = 1, movingFriction = 1, reflection = -1;
        float gravity = 1, ladderGravity = 0, acceleration = 0, reversal = 1;
        float directionalForce = 0, maximumSpeed = 0, threshold = 0;
        bool applyDrag = true, applyFriction = true, applyReflection = false;
        bool applyGravity = true, applyLadderGravity = false, inverted = false;
        bool applyMotion = false, applyDirection = false, emitSound = false;
    };

    Vector updateVelocity(const Vector& velocity, const VelocityInput& input, const mapcollision::State& collision,
        const std::function<void(std::uint32_t, const Vector&)>& sound);
}
