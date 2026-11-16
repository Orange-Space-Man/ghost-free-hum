#pragma once

#include "game/actors/moveactor.h"
#include "game/player/playereventdispatch.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"

namespace enemystrategy {
    unsigned notifyScare(const actors::MoveActor& actor, unsigned player, const roomlocation::Rooms& rooms,
        const roomtraversal::State& visibility, const playerevents::Dispatcher& events);
}
