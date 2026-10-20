#pragma once

#include "billscattercontact.h"
#include "game/actors/actorinitialpose.h"

namespace billscatter {
    struct MotionParameters {
        float maximumScale = 0.75f, drag = 0.36f, accelerationDamping = 0.95f;
        float flutterAlignment = 4, flutterVariation = 0.5f, gravity = 1.5f, settlingSpeed = 2.3f;
    };
    Vector flutterDirection(std::uint32_t& randomState, const actormodel::InitialPose& pose);
    void move(Particle& particle, const Request& request, bool ordinaryMotion, float flutterStrength,
        std::uint8_t flutterThreshold, std::uint32_t& particleRandom, std::uint32_t& gameplayRandom,
        const actormodel::InitialPose& pose, ContactServices& services, const MotionParameters& parameters = {});
}
