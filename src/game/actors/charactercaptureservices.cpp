#include "game/actors/charactercaptureservices.h"
#include "game/enemies/itemcreationroute.h"
#include "game/player/playeractor.h"

#include <stdexcept>

characterowner::NativeCaptureServices::NativeCaptureServices(actorcapture::Motion& motion,
    playerevents::Dispatcher& players, characteritems::Creation& items) : motion(motion), players(players), items(items) {
}

void characterowner::NativeCaptureServices::initializeMotion(Owner& owner, std::uint16_t type) {
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    const auto index = owner.manager().nearestPlayer(actor.position);
    const auto* player = players.player(index);
    if (!player || !player->view.field8A4)
        throw std::runtime_error("Original capture initialization requires the nearest player's vacuum target.");
    actor.initializeCapture(motion, *player->view.field8A4, type);
}

void characterowner::NativeCaptureServices::dropItem(Owner& owner) {
    items.drop(owner);
}

void characterowner::NativeCaptureServices::notifyPlayer(Owner& owner) {
    playerevents::Payload payload;
    payload.word(0x78, owner.actor().type());
    players.send(0, 0x15, &payload);
}
