#include "game/player/playerbombdamage.h"
#include "game/player/playeractorcomponents.h"

bool playerevents::bombDamage(playeractor::Player& player, std::uint32_t damage, std::uint32_t mansionMode,
    LifeEnvironment& environment) {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    if ((actions.field310 == 2 && actions.field314 == 0x20) || !(state.field10C4 <= 0) ||
        static_cast<std::int16_t>(player.life) <= 0 || state.flags1040[2] || state.field1150 != 0 ||
        (state.field11A0 && (state.field11A0->flags & 0x40)) || state.field1060 ||
        (actions.field310 == 2 && actions.field35C == 4 && (actions.field314 == 0xe || actions.field314 == 0xc))) return false;
    if (state.field1170 != 0) {
        if (state.field1178 >= 1) damage = static_cast<std::uint32_t>(static_cast<std::int32_t>(damage) >> 1);
        else damage <<= 1;
    }
    const auto loss = mansionMode ? damage << 1 : damage;
    setLife(player, static_cast<std::int16_t>(std::uint32_t(static_cast<std::int32_t>(static_cast<std::int16_t>(player.life))) - loss), environment);
    environment.sound(0x2810, player.position);
    if (static_cast<std::int32_t>(damage) > 0) state.field10C4 = 120;
    auto& reaction = *player.components->reactions;
    reaction.state = 1;
    reaction.field4 = 0;
    reaction.field8 = 0;
    reaction.fieldC = {};
    reaction.field18 = 0;
    state.field1338 = 2;
    state.field133C = 2;
    return true;
}
