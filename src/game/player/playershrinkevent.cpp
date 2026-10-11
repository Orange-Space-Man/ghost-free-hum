#include "game/player/playershrinkevent.h"
#include "game/player/playeractorcomponents.h"

unsigned playerevents::shrink(playeractor::Player& player, float ratio, std::uint32_t frames, ShrinkEnvironment& environment) {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const bool blocked = actions.field310 == 2 && actions.field35C == 4 &&
        (actions.field314 == 0xe || actions.field314 == 0xc);
    if (!state.flags1140[2] && !blocked && state.field1170 == 0) {
        state.field1170 = static_cast<float>(static_cast<std::int32_t>(frames));
        state.field1174 = static_cast<float>(environment.transitionFrames);
        state.field1178 = ratio;
        environment.life.sound(0x847, player.position);
        environment.audioScale = 2;
        loseMoney(player, 10, environment.life);
    }
    return 0;
}
