#include "resources/dolresources.h"
#include "game/player/playercontrolframe.h"
#include "game/startup/startupresources.h"

#include <cstring>
#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    bool grounded(const playeractor::Player& player) {
        const auto& actions = *player.components->actions;
        return (player.fieldA0 >= 0 || player.fieldB2 >= 0) &&
            !(actions.field310 == 2 && (actions.field314 == 32 || actions.field314 == 33));
    }
}

playercontrols::Frame::Frame(const std::vector<std::uint8_t>& executable) {
    const auto bits = word(executable, 0x804db598);
    std::memcpy(&attackDuration, &bits, 4);
    decayAmount = static_cast<std::int32_t>(word(executable, 0x804d2c1c));
    decayDelay = static_cast<std::int32_t>(word(executable, 0x804d2c20));
}

void playercontrols::Frame::charge(playeractor::Player& player, const playerevents::LifeEnvironment& environment) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto& controls = *player.components->controls;
    if (actions.field310 == 1 || (actions.field310 == 2 &&
        (actions.field314 == 18 || actions.field314 == 10 || actions.field314 == 12))) {
        if (player.components->reactions->state != 4 && grounded(player)) {
            if (state.field1114 > 0 ? controls.flagsC[14] : controls.flagsC[13]) {
                if (state.field1114 <= 0 && controls.flagsC[13]) environment.sound(0x2814, player.position);
                state.field1114 = 1;
            }
        }
    }
    if (state.field1114 > 0) {
        state.field1114 -= 1;
        if (state.field1114 <= 0 && controls.flagsC[14]) {
            state.field1114 = 1;
            if (state.field1110 > 0) environment.sound(0x2805, player.position);
        }
    }
}

void playercontrols::Frame::attack(playeractor::Player& player) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto& controls = *player.components->controls;
    if ((actions.field310 == 1 || (actions.field310 == 2 && actions.field314 == 18)) &&
        !actions.query74968() && controls.flagsC[12]) {
        state.field110C = attackDuration;
        state.field1114 = 0;
    }
    if (state.field110C > 0) {
        state.field110C -= 1;
        if (state.field110C <= 0 && controls.flagsC[12]) state.field110C = 1;
    }
}

void playercontrols::Frame::blink(playeractor::Player& player, unsigned frame) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    if (state.field10C4 > 0 && actions.field310 == 1 && actions.fields31C[4] != 10 && actions.fields31C[4] != 13) {
        state.field10C4 -= 1;
        state.fieldF5C = frame & 1 ? 127 : 255;
    } else state.fieldF5C = 255;
}

void playercontrols::Frame::special(playeractor::Player& player, unsigned frame, bool doubledDamage,
    std::uint8_t& specialActive, playerevents::LifeEnvironment& environment) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    switch (state.field1338) {
    case 2:
        if (static_cast<std::int32_t>(state.field133C) > 0 && actions.fields31C[4] == 10 && (player.flags & 0x20000))
            --state.field133C;
        if (static_cast<std::int32_t>(state.field133C) < 1) {
            state.field133C = 0;
            state.field1338 = 1;
        }
        break;
    case 3:
        if (actions.field310 != 2 || actions.field314 != 23) {
            state.field1338 = 1;
            specialActive = 0;
        }
        break;
    case 4:
        if (state.field1340 > 0) {
            if (frame % 10 == 0) playerevents::setLife(player,
                static_cast<std::int16_t>(player.life) - (doubledDamage ? 2 : 1), environment);
            if (!player.components->controls->flagsC[16]) state.field1344 += 1;
            else {
                state.field1340 -= 1;
                state.field10C8 = 2;
                state.field10CC = 0.5f;
                environment.sound(0x814, player.position);
                state.field1344 = 0;
            }
            if (float(decayDelay) <= state.field1344) state.field1340 -= float(decayAmount) / 50;
        }
        if (state.field1340 <= 0) {
            state.field1340 = 0;
            state.field1338 = 1;
        }
        break;
    }
}
