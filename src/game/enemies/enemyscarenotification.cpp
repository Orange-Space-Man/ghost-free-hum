#include "game/enemies/enemyscarenotification.h"

unsigned enemystrategy::notifyScare(const actors::MoveActor& actor, unsigned player,
    const roomlocation::Rooms& rooms, const roomtraversal::State& visibility, const playerevents::Dispatcher& events) {
    const auto room = rooms.resolve(actor.position, actor.fieldB4);
    if (!(visibility.record(room, 0).flags & 1)) return 0;
    playerevents::Payload payload;
    payload.word(8, actor.id());
    payload.word(12, 0);
    return events.send(static_cast<std::int32_t>(player), 2, &payload);
}
