#pragma once

#include "moneyscatterinitializer.h"
#include "game/player/playereventpayload.h"
#include "game/world/mapcollisionstate.h"

namespace moneyscatter {
    struct ContactServices {
        virtual ~ContactServices() = default;
        virtual mapcollision::SphereContact sweep(const Vector& position, float radius, const Vector& velocity) = 0;
        virtual Vector& playerPosition() = 0;
        virtual float playerTop() = 0;
        virtual void playerEvent(unsigned event, const playerevents::Payload& payload) = 0;
        virtual void collectionEffect(const Vector& position) = 0;
        virtual std::uint32_t roomFlags(std::uint32_t room) = 0;
        virtual void notifyPlacedCollection(std::uint32_t room) = 0;
    };

    struct CollisionResult {
        bool audible = false, wall = false;
    };

    void move(Particle& particle, bool ordinaryMotion);
    CollisionResult collide(Particle& particle, bool ordinaryMotion, const actormodel::InitialPose& pose,
        ContactServices& services, const std::optional<double>& retainedFoldedSpin = std::nullopt);
    bool collect(const Request& request, Particle& particle, bool wall, ContactServices& services);
    void collectSuction(Pool& pool, Request& request, Particle& particle, ContactServices& services);
}

