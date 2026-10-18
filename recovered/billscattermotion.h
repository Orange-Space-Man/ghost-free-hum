#pragma once

#include "billscattercontact.h"
#include "game/actors/actorinitialpose.h"

namespace billscatter {
    Vector flutterDirection(std::uint32_t& randomState, const actormodel::InitialPose& pose);
    void move(Particle& particle, const Request& request, bool ordinaryMotion, float flutterStrength,
        std::uint8_t flutterThreshold, std::uint32_t& particleRandom, std::uint32_t& gameplayRandom,
        const actormodel::InitialPose& pose, ContactServices& services);
}
