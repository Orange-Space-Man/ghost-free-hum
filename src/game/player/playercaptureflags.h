#pragma once

#include "game/actors/charactercreationstate.h"
#include "game/player/playercomponentstate.h"

namespace playerevents {
    bool registerCapture(playercomponents::BitBuffer& captured, playercomponents::BitBuffer& grades,
        const charactercreation::Properties& properties, unsigned type, std::uint16_t life);
    bool registerSeen(playercomponents::BitBuffer& seen, unsigned type);
    void promoteGrade(playercomponents::BitBuffer& grades, std::int32_t index, std::int32_t grade);
}
