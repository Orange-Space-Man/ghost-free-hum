#pragma once

#include "game/events/entrancescript.h"
#include "game/actors/moveactor.h"

namespace gameevent {
    class ActorServices {
    public:
        virtual ~ActorServices() = default;
        virtual actors::MoveActor* find(std::string_view name) = 0;
        virtual void lookPlayer(actors::MoveActor& target) = 0;
        virtual void sound(actors::MoveActor& actor, unsigned index) = 0;
    };

    void applyActorCommand(actors::MoveActor& actor, const ActorCommand& command, ActorServices& services);
}
