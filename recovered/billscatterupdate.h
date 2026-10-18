#pragma once

#include "billscattermotion.h"
#include "billscattercollection.h"

namespace billscatter {
    struct UpdateServices : ContactServices, CollectionServices {
        virtual int suction(Particle& particle) = 0;
        virtual Vector suctionPosition() = 0;
        virtual void updateModel(Particle& particle) = 0;
    };

    void advanceOrientation(Particle& particle, bool ordinaryMotion, const actormodel::InitialPose& pose);
    bool advance(Pool& pool, Request& request, Particle& particle, float flutterStrength,
        std::uint8_t flutterThreshold, std::uint32_t& particleRandom, std::uint32_t& gameplayRandom,
        const actormodel::InitialPose& pose, UpdateServices& services);
}
