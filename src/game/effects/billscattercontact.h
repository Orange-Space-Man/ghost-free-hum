#pragma once

#include "billscatterpool.h"
#include "game/world/mapcollisionstate.h"

namespace billscatter {
    struct ContactServices {
        virtual ~ContactServices() = default;
        virtual mapcollision::SphereContact sweep(const Vector& position, float radius, const Vector& velocity) = 0;
    };

    void collide(Particle& particle, float radius, float settlingSpeed, ContactServices& services);
}
