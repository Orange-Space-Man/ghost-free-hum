#pragma once

#include "playerscannerinteraction.h"
#include "game/actors/actorregistry.h"
#include "game/world/roomtraversalstate.h"

namespace playerscanner {
    class ActorBindings {
    public:
        ActorBindings(enemies::Manager& manager, const actors::Registry& actors, const actormodel::Catalog& models,
            const actorcollision::Catalog& collisions, const roomtraversal::State& visibility);
        Hit query(const Vector& position, const Vector& direction) const;
        unsigned type(unsigned actor) const;
        std::optional<unsigned> message(unsigned actor) const;

    private:
        enemies::Manager& manager;
        const actors::Registry& actors;
        const actormodel::Catalog& models;
        const actorcollision::Catalog& collisions;
        const roomtraversal::State& visibility;
    };
}
