#include "game/player/playersuctioncompletion.h"
#include "game/player/playerattractionevent.h"
#include "game/player/playermoneycollection.h"

#include <stdexcept>
#include <utility>

namespace {
    void collectionBit(playercomponents::BitBuffer& bits, std::int32_t index, gameflags::State& flags) {
        if (!bits.data) return;
        const auto byte = static_cast<unsigned>(index) >> 3;
        if (byte >= bits.bytes || bits.bytes < 8) throw std::runtime_error("Suction collection bit exceeds original bound storage.");
        bits.data[byte] |= static_cast<std::uint8_t>(1u << (index & 7));
        unsigned count = 0;
        for (unsigned bit = 0; bit < 60; ++bit) if (bits.data[bit >> 3] & (1u << (bit & 7))) ++count;
        if (count > 4) flags.setOn(0x38);
    }
}

bool playerevents::completeSuction(playeractor::Player& player, const itemsuction::Completion& payload,
    const SuctionEnvironment& environment) {
    auto& state = player.state;
    auto& components = *player.components;
    auto& weapon = *components.weapon;
    bool result = false;
    bool refresh = false;
    std::int32_t delay = 10;
    const auto reaction = [&](unsigned kind) {
        components.actions->suctionReaction(components.reactions->state, kind);
    };
    switch (payload.kind) {
    case 0:
    case 1:
        state.fields11D8[payload.kind] += payload.amount;
        state.field1120 = 8;
        result = true;
        break;
    case 2:
        state.fields11D8[2] += payload.amount;
        environment.sounds.actor(0x1038, player.position, 0);
        result = true;
        break;
    case 3: {
        unsigned element = 1;
        if (payload.type == 0x72) element = 2;
        else if (payload.type == 0x73 || payload.type == 0xd6) element = 4;
        else if (payload.type == 0x74) element = 3;
        if (element != 1) {
            state.field1188 = static_cast<float>(environment.elementDuration);
            state.field1184 = element;
        }
        const float size = static_cast<float>(environment.catalog.type(payload.type).field14);
        if (state.entryCount == 64) {
            for (unsigned i = 0; i < 63; ++i) state.entries[i] = state.entries[i + 1];
            --state.entryCount;
        }
        state.entries.at(state.entryCount) = payload.type;
        ++state.entryCount;
        delay = weapon.pipe->pulse(-0.7f, 2 * size);
        environment.sounds.weapon(size > 2 ? 0x4828 : 0x4808, weapon.first->position);
        player.playFeedback(size > 1 ? (size > 2 ? 14 : 13) : 12);
        if (payload.collectionBit >= 0) {
            if (payload.type != 0xd6) reaction(4);
            collectionBit(*components.bits[2], payload.collectionBit, environment.flags);
            refresh = true;
        }
        if (payload.type == 0xdd) state.field10C4 = state.field117C = 600;
        if (payload.type == 0xdc) reaction(3);
        registerCapture(*components.bits[0], *components.bits[4], environment.properties, payload.type, player.life);
        result = true;
        break;
    }
    case 4:
        state.field12F0 += payload.amount;
        result = true;
        break;
    case 5:
    case 6:
        delay = collectSuctionMoney(player, payload.kind, payload.amount,
            [&](unsigned id, const auto& position) { environment.sounds.actor(id, position, 0); });
        result = true;
        break;
    case 7:
    case 8:
        weapon.pipe->pulse(-0.7f, 3);
        player.playFeedback(12);
        environment.sounds.weapon(0x4808, weapon.first->position);
        reaction(0);
        break;
    }
    if (payload.delayEnabled == 1 && state.field1124 <= 0) {
        result = true;
        state.field1124 = static_cast<float>(delay);
        state.field1128 = payload.delay;
    }
    if (refresh) player.refreshView();
    return result;
}

playerevents::SuctionEvents::SuctionEvents(playeractor::Player& player, const actors::Registry& actors,
    const SuctionEnvironment& environment, std::function<void(actors::MoveActor&)> collector) :
    player(player), actors(actors), environment(environment), collector(std::move(collector)) {
    if (!this->collector) throw std::runtime_error("Suction event binding requires its original owner collection callback.");
}

void playerevents::SuctionEvents::attraction(unsigned actor) {
    beginAttraction(player, actors, nullptr, static_cast<std::int32_t>(actor), 10);
}

void playerevents::SuctionEvents::completion(const itemsuction::Completion& payload) {
    completeSuction(player, payload, environment);
}

void playerevents::SuctionEvents::collect(actors::MoveActor& actor) {
    collector(actor);
}
