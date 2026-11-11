#include "game/player/playereventdispatch.h"
#include "game/player/playeractor.h"
#include "game/player/playersuctioncompletion.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>
#include <utility>

unsigned playerevents::dispatch(playeractor::Player& player, unsigned message, const Payload* payload,
    const CollectionSound& collectionSound, const SuctionEnvironment* suction) {
    if (!message || message > 0x40) return 0;
    if (message == 4) {
        player.state.field106C = -1.0f;
        return 1;
    }
    if (message == 5) {
        if (player.state.field106C == 0) return 0;
        player.state.field106C = 0;
        return 1;
    }
    if (message == 0x3a) {
        player.flags &= ~0x40u;
        return 0;
    }
    const Payload defaults;
    const auto& values = payload ? *payload : defaults;
    if (message == 7 || message == 8) {
        if (values.word(0x2c) || static_cast<std::int32_t>(values.word(0x30)) >= 0)
            throw std::runtime_error("Player attention requires a resolved native target.");
        if (!values.lookPosition) return 0;
        player.setAttention(*values.lookPosition, message == 7 ? 5 : 6);
        return 1;
    }
    if (message == 6) {
        const auto frames = static_cast<std::int32_t>(values.word(0x28));
        if (frames <= 0) return 0;
        player.state.field106C = static_cast<float>(frames);
        return 1;
    }
    if (message >= 0x1b && message <= 0x1e) {
        auto& flag = message <= 0x1c ? player.state.flags1330[3] : player.state.field1334;
        const bool enabled = message == 0x1b || message == 0x1d;
        if ((flag != 0) == enabled) return 0;
        flag = enabled ? 1 : 0;
        return 1;
    }
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
    if (recipient && message == 3) {
        if (!damage) throw std::logic_error("Player damage dispatch requires its bound life services.");
        const Payload defaults;
        const auto& values = payload ? *payload : defaults;
        const auto* position = values.damagePosition;
        const auto source = static_cast<std::int32_t>(values.word(0x14));
        if (source >= 0) {
            const auto* actor = actors.actor(source);
            if (!actor) throw std::runtime_error("Player damage source is absent from the actor registry.");
            position = &actor->position;
        } else if (values.word(0x10)) {
            throw std::runtime_error("Player damage requires a native source position.");
        }
        if (!position) throw std::runtime_error("Player damage has no source position.");
        float facing = 0;
        for (unsigned axis = 0; axis < 3; ++axis)
            facing += ((*position)[axis] - recipient->position[axis]) * recipient->state.direction[axis];
        const bool behind = facing < 0;
        if (!damage(*recipient, values, behind) || values.word(0x20) == 7 ||
            (recipient->fieldA0 < 0 && recipient->fieldB2 < 0)) return 0;
        auto attention = values;
        attention.look(position, source);
        send(index, behind ? 8 : 7, &attention);
        return 1;
    }
    if (recipient && (message == 7 || message == 8) && payload && static_cast<std::int32_t>(payload->word(0x30)) >= 0) {
        auto resolved = *payload;
        const auto* target = actors.actor(payload->word(0x30));
        if (!target) throw std::runtime_error("Player attention actor is absent from the original registry.");
        resolved.look(&target->position);
        return dispatch(*recipient, message, &resolved, collectionSound, suction);
    }
    return recipient ? dispatch(*recipient, message, payload, collectionSound, suction) : 0;
}

void playerevents::Dispatcher::bindDamage(std::function<bool(playeractor::Player&, const Payload&, bool)> handler) {
    damage = std::move(handler);
}
