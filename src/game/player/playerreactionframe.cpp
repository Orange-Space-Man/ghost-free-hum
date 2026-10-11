#include "game/player/playerreactionframe.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

void playeractions::advanceState(Reactions& state, Actions* actions, bool enabled, bool silent,
    const std::array<float, 3>& position, const std::function<void(unsigned, const std::array<float, 3>&)>& sound) {
    if (state.state == 1 || !(state.field4 > 0) || !enabled) return;
    state.field4 -= 1;
    if (!(state.field4 <= 0)) return;
    if (actions && !silent) {
        if (state.state == 4) {
            std::array<std::uint32_t, 12> payload{};
            payload[11] = 1;
            actions->request(10, &payload);
        } else if (state.state == 2 || state.state == 3) actions->request(1);
    }
    state.state = 1;
    state.field4 = 0;
    state.field8 = 0;
    state.fieldC = {};
    state.field18 = 0;
    if (!silent) sound(0x281c, position);
}

void playeractions::updateState(playeractor::Player& player, playerevents::LifeEnvironment& environment) {
    auto& state = *player.components->reactions;
    auto& actions = *player.components->actions;
    advanceState(state, &actions, !(actions.field310 == 2 && actions.field314 == 3), false, player.position, environment.sound);
    const auto* record = state.entries.at(state.state);
    if (!record) throw std::runtime_error("Player reaction frame requires its original state record.");
    float factor;
    std::memcpy(&factor, &record->at(2), 4);
    const float scaled = float(static_cast<std::int16_t>(player.state.maximumLife)) * factor;
    const auto integer = !std::isfinite(scaled) || double(scaled) < -2147483648.0 || double(scaled) >= 2147483648.0 ?
        std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(scaled);
    player.state.maximumLife = static_cast<std::uint16_t>(integer);
    const auto maximum = static_cast<std::int16_t>(player.state.maximumLife);
    const auto life = static_cast<std::int16_t>(player.life);
    playerevents::setLife(player, life < maximum ? life : maximum, environment);
    if (state.state == 1) player.state.field1000 = 50;
    else {
        if (!std::isfinite(state.field4) || state.field4 < 0 || double(state.field4) >= 4294967296.0)
            throw std::runtime_error("Player reaction duration exceeds the original unsigned clock range.");
        player.state.field1000 = 50 + (float(static_cast<std::uint32_t>(state.field4)) / float(record->at(1))) * 50;
    }
}
