#include "game/world/doorstate.h"
#include "game/player/playereventdispatch.h"

void doors::State::setGlobalBarrier(bool enabled, const playerevents::Dispatcher& events) {
    setGlobalBarrier(enabled, [&](unsigned player, unsigned message, unsigned kind) {
        playerevents::Payload payload;
        payload.word(0x50, kind);
        events.send(static_cast<std::int32_t>(player), message, &payload);
    });
}
