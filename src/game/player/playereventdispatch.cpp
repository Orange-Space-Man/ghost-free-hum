#include "game/player/playereventdispatch.h"
#include "game/player/playeractor.h"
#include "game/player/playersuctioncompletion.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>
#include <utility>

unsigned playerevents::dispatch(playeractor::Player& player, unsigned message, const Payload* payload,
    const CollectionSound& collectionSound, const SuctionEnvironment* suction) {
    if (!message || message > 0x40) return 0;
    const Payload defaults;
    const auto& values = payload ? *payload : defaults;
    if (message == 0x34 || message == 0x35) {
        if (values.word(0xec) || values.word(0xf0))
            throw std::runtime_error("Player pull events require native references rather than encoded game addresses.");
        auto& state = player.state;
        if (message == 0x34) {
            if (!values.pullCenter || (state.flags134C[2] &&
                !(!values.pullOwner && state.pullOwner) && !(values.pullOwner && state.pullOwner == values.pullOwner))) return 0;
            state.flags134C[2] = 1;
            state.field1350 = *values.pullCenter;
            state.pullOwner = values.pullOwner;
            state.field1360 = values.number(0xf4);
            state.field1364 = values.number(0xf8);
            return 1;
        }
        if (!state.flags134C[2] || (state.pullOwner && state.pullOwner != values.pullOwner)) return 0;
        state.flags134C[2] = 0;
        state.pullOwner = nullptr;
        return 1;
    }
    if (message == 0x26 || message == 0x27 ||
        (message == 0x13 && (values.word(0x60) == 5 || values.word(0x60) == 6)))
        return collectMoney(player, message, values, collectionSound);
    if (message == 0x13 && (values.word(0x60) == 3 || values.word(0x60) == 7)) {
        if (!suction) throw std::runtime_error("Player suction completion services are not bound.");
        return completeSuction(player, {values.word(0x60), values.word(0x64), values.word(0x68),
            static_cast<std::int32_t>(values.word(0x6c)), values.word(0x70), values.word(0x74)}, *suction);
    }
    if (message != 0x12) throw std::runtime_error("Player event is outside the ported first-area dispatch path.");
    const auto kind = values.word(0x50);
    if (kind >= 1 && kind <= 3) player.playFeedback(0x11 + kind);
    else player.pulseFeedback(values.number(0x54), values.word(0x58), values.word(0x5c));
    return 1;
}

playerevents::Dispatcher::Dispatcher(enemies::Manager& manager, actors::Registry& actors, playeractor::Player* alternate,
    CollectionSound collectionSound, const SuctionEnvironment* suction) :
    manager(manager), actors(actors), alternate(alternate), collectionSound(std::move(collectionSound)), suction(suction) {
}

playeractor::Player* playerevents::Dispatcher::player(std::int32_t index) const {
    auto* player = alternate;
    if (index < 0x100) {
        if (!manager.actorsInitialized()) throw std::runtime_error("Player events require initialized EnManager actors.");
        if (index < 0 || static_cast<unsigned>(index) >= manager.playerCount()) throw std::out_of_range("Original player event index.");
        player = dynamic_cast<playeractor::Player*>(actors.actor(manager.appearPoint(index).actor));
    }
    return player;
}

unsigned playerevents::Dispatcher::send(std::int32_t index, unsigned message, const Payload* payload) const {
    auto* recipient = player(index);
    return recipient ? dispatch(*recipient, message, payload, collectionSound, suction) : 0;
}
