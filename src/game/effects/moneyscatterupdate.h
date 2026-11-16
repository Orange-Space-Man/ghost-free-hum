#pragma once

#include "moneyscattercontact.h"

namespace moneyscatter {
    struct UpdateState {
        bool ordinaryMotion = true;
    };

    struct UpdateServices : ContactServices {
        virtual int suction(float radius, Particle& particle) = 0;
        virtual Vector suctionPosition() = 0;
        virtual float suctionMinimumScale() = 0;
        virtual void bounceSound(const Vector& position) = 0;
        virtual void sparkle(const Vector& position) = 0;
        virtual void updateModel(Particle& particle) = 0;
    };

    void approachAngles(Particle& particle, const std::array<std::int16_t, 3>& target,
        bool ordinaryMotion, const actormodel::InitialPose& pose);
    void advanceOrientation(Particle& particle, bool ordinaryMotion, const actormodel::InitialPose& pose);
    bool advance(Pool& pool, Request& request, Particle& particle, UpdateState& state,
        const actormodel::InitialPose& pose, UpdateServices& services,
        const std::optional<double>& retainedFoldedSpin = std::nullopt);
}
