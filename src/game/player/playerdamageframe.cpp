#include "game/player/playerdamageframe.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <stdexcept>

playerevents::DamageFrame::DamageFrame(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::playerDamageDelays, 8, bytes, error)) throw std::runtime_error(error);
    const auto read = [&](unsigned offset) {
        return static_cast<std::int32_t>((unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3]);
    };
    ordinaryDelay = read(0);
    reactionDelay = read(4);
}

bool playerevents::DamageFrame::run(playeractor::Player& player, const Payload& payload, bool alternateReaction,
    const actors::Registry& actors, const actormodel::Catalog& catalog, bool doubledDamage, LifeEnvironment& life) const {
    auto& state = player.state;
    auto& actions = *player.components->actions;
    const auto sourceId = static_cast<std::int32_t>(payload.word(0x14));
    const auto* source = sourceId >= 0 ? actors.actor(sourceId) : nullptr;
    if (!(state.field10C4 <= 0) || static_cast<std::int16_t>(player.life) <= 0 || state.flags1040[2] || state.field1150 != 0 ||
        (state.field11A0 && (state.field11A0->flags & 0x40)) ||
        (actions.field310 == 2 && actions.field314 == 0x20 && static_cast<std::int32_t>(state.field112C) <= 0) ||
        (actions.field310 == 2 && actions.field35C == 4 && (actions.field314 == 0xe || actions.field314 == 0xc)) ||
        state.field1060) return false;
    if (!(state.field1114 <= 0) && !payload.bytes[0x1c] && source &&
        catalog.type(source->actorType).field14 != 3 && !(source->flags & 0x10000)) return false;
    auto amount = payload.word(0x18);
    if (state.field1170 != 0) {
        if (state.field1178 >= 1) amount = static_cast<std::uint32_t>(static_cast<std::int32_t>(amount) >> 1);
        else amount <<= 1;
    }
    const auto kind = payload.word(0x20);
    if (kind == 7) state.field112C = amount;
    else {
        const auto loss = doubledDamage ? amount << 1 : amount;
        setLife(player, static_cast<std::int16_t>(std::uint32_t(static_cast<std::int32_t>(static_cast<std::int16_t>(player.life))) - loss), life);
        if (kind) life.sound(0x2810, player.position);
        if (static_cast<std::int32_t>(amount) > 0) state.field10C4 = static_cast<float>(kind > 1 && kind < 4 ? reactionDelay : ordinaryDelay);
    }
    auto& reaction = *player.components->reactions;
    reaction.state = 1;
    reaction.field4 = 0;
    reaction.field8 = 0;
    reaction.fieldC = {};
    reaction.field18 = 0;
    if (kind != 0 && kind != 1) {
        constexpr std::array<unsigned, 8> reactions{0, 0, 1, 2, 3, 5, 6, 4};
        if (kind >= reactions.size()) throw std::runtime_error("Unsupported original player damage reaction.");
        std::array<std::uint32_t, 12> request{};
        request[1] = reactions[kind];
        const auto direction = payload.word(0x24);
        if (direction == 0) request[3] = 1;
        else if (direction == 1) request[3] = 2;
        request[4] = alternateReaction ? 2 : 1;
        actions.request(3, &request);
    }
    return true;
}
