#pragma once

#include "game/actors/characterownership.h"
#include "game/actors/moveactor.h"
#include "game/audio/jaudiopositionowners.h"

namespace characterownership {
    class Links final : public ActorLinks {
    public:
        Links(actors::MoveActor& actor, enemies::Manager& manager, jaudio::PositionOwners& sounds);
        void setInactiveFlags() override;
        void* child(unsigned index) override;
        void detach(void* child) override;
        void deactivate(void* child) override;
        void releaseAudio() override;

    private:
        actors::MoveActor& actor;
        enemies::Manager& manager;
        jaudio::PositionOwners& sounds;
    };
}
