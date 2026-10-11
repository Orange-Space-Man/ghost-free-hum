#include "resources/dolresources.h"
#include "game/player/playertimerframe.h"
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

    void decrement(float& value, float amount = 1) {
        if (value > 0) {
            value -= amount;
            if (value < 0) value = 0;
        }
    }
}

playeractor::TimerFrame::TimerFrame(const std::vector<std::uint8_t>& executable) {
    powerDelay = static_cast<std::int32_t>(word(executable, 0x804d2c64));
    escapePulse = static_cast<std::int32_t>(word(executable, 0x804d2c3c));
    escapeDelay = static_cast<std::int32_t>(word(executable, 0x804d2c40));
    pulseColor = word(executable, 0x804db594);
    const auto bits = word(executable, 0x804db580);
    std::memcpy(&escapeDecay, &bits, 4);
}

void playeractor::TimerFrame::run(Player& player, const TimerServices& services) const {
    auto& state = player.state;
    const auto& controls = *player.components->controls;
    const auto damage = [&] {
        playerevents::setLife(player, static_cast<std::int16_t>(player.life) - (services.doubledDamage ? 2 : 1), services.life);
    };
    decrement(state.field106C);
    decrement(state.field1174);
    if (state.field1170 > 0 && state.field1174 == 0) {
        state.field1170 -= 1;
        if (state.field1170 <= 0) {
            state.field1170 = 0;
            state.field1174 = float(powerDelay);
            services.life.sound(0x846, player.position);
            services.changeAudioRate(false);
        }
    }
    decrement(state.field118C);
    decrement(state.field105C);
    if (state.field1124 > 0) {
        state.field1124 -= 1;
        if (state.field1124 <= 0) {
            std::array<float, 3> origin;
            for (unsigned i = 0; i < 3; ++i) origin[i] = player.position[i] - state.direction[i];
            services.delayedDamage(player, origin, state.field1128);
            services.weaponPulse(player, pulseColor, 300, 99, 15);
            services.life.sound(0x2809, player.position);
            services.life.sound(0x80a, player.position);
            services.matrixEffect(4, state.matrix1010);
        }
    }
    if (state.field1130 <= 0) state.field1138 = 0;
    else {
        if (!state.flags1140[1] && services.frame % 10 == 0) damage();
        if (!controls.flagsC[16]) state.field113C += 1;
        else {
            state.field1130 -= 1;
            state.field1138 = float(escapePulse);
            state.field113C = 0;
        }
        if (!state.flags1140[1] && state.field113C >= float(escapeDelay)) {
            state.field1130 -= escapeDecay;
            state.flags1140[0] = 0;
        } else state.flags1140[0] = 1;
        if (state.field1130 < 0) state.field1130 = 0;
    }
    decrement(state.field1138);
    decrement(state.field10C8, state.field10CC);
    if (state.field1348 > 0) {
        state.field1348 -= 1;
        if (services.frame % 5 == 0) damage();
        if (state.field1348 < 0) state.field1348 = 0;
    }
    decrement(state.field117C);
    if (player.components->actions->field310 == 1 && !player.components->attached->field2C0 &&
        !state.flags1140[2] && controls.field20 == 0 && controls.field2C == 0 && !state.field11A0 &&
        (!state.fieldFB8 || state.fieldFB8->value == 0) && (!state.fieldFBC || state.fieldFBC->value == 0)) state.field116C += 1;
    else state.field116C = 0;
}
