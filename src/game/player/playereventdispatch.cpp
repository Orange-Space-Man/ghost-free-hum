#include "game/player/playereventdispatch.h"
#include "game/player/playeractor.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>
#include <utility>

unsigned playerevents::dispatch(playeractor::Player& player, unsigned message, const Payload* payload,
    const CollectionSound& collectionSound) {
    if (!message || message > 0x40) return 0;
    const Payload defaults;
    const auto& values = payload ? *payload : defaults;
    if (message == 0x26 || message == 0x27 ||
        (message == 0x13 && (values.word(0x60) == 5 || values.word(0x60) == 6)))
        return collectMoney(player, message, values, collectionSound);
    if (message != 0x12) throw std::runtime_error("Player event is outside the ported first-area dispatch path.");
    const auto kind = values.word(0x50);
    if (kind >= 1 && kind <= 3) player.playFeedback(0x11 + kind);
    else player.pulseFeedback(values.number(0x54), values.word(0x58), values.word(0x5c));
    return 1;
}

playerevents::Dispatcher::Dispatcher(enemies::Manager& manager, actors::Registry& actors, playeractor::Player* alternate,
    CollectionSound collectionSound) :
    manager(manager), actors(actors), alternate(alternate), collectionSound(std::move(collectionSound)) {
}

unsigned playerevents::Dispatcher::send(std::int32_t index, unsigned message, const Payload* payload) const {
    auto* player = alternate;
    if (index < 0x100) {
        if (!manager.actorsInitialized()) throw std::runtime_error("Player events require initialized EnManager actors.");
        if (index < 0 || static_cast<unsigned>(index) >= manager.playerCount()) throw std::out_of_range("Original player event index.");
        player = dynamic_cast<playeractor::Player*>(actors.actor(manager.appearPoint(index).actor));
    }
    return player ? dispatch(*player, message, payload, collectionSound) : 0;
}
