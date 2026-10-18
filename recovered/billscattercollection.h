#pragma once

#include "billscatterpool.h"
#include "game/player/playereventpayload.h"

namespace billscatter {
    struct CollectionServices {
        virtual ~CollectionServices() = default;
        virtual Vector playerPosition() = 0;
        virtual float playerTop() = 0;
        virtual void playerEvent(unsigned event, const playerevents::Payload& payload) = 0;
    };

    void scaleSuction(Particle& particle, const Vector& suctionPosition);
    bool collect(Particle& particle, CollectionServices& services);
    void collectSuction(Pool& pool, Request& request, Particle& particle, CollectionServices& services);
}
