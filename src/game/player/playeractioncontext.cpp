#include "resources/dolresources.h"
#include "game/player/playeractioncontext.h"
#include "game/startup/startupresources.h"
#include "game/actors/actortimingsampler.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T, std::size_t N>
    void read(const std::vector<std::uint8_t>& executable, unsigned address, std::array<T, N>& destination) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, N * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < N; ++i) {
            const unsigned offset = i * 4;
            const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
                (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
            std::memcpy(&destination[i], &bits, 4);
        }
    }
}

playeractions::Context::Context(const std::vector<std::uint8_t>& executable) {
    read(executable, 0x804d2b38, thresholds);
    read(executable, 0x804d2bb8, speeds);
    read(executable, 0x804d2c4c, idleThresholds);
    std::array<float, 1> value;
    read(executable, 0x804db52c, value); turnFactor = value[0];
    read(executable, 0x804db57c, value); reactionFactor = value[0];
}

float playeractions::Context::maximumSpeed(const playeractor::Player& player) const {
    const auto& state = player.state;
    float value;
    if (state.flags134C[0]) value = speeds[4];
    else if (state.flags134C[1]) value = speeds[5];
    else if (state.field1180 == 1 || state.field1180 == 6) value = speeds[2];
    else if (state.field1114 > 0) value = speeds[3];
    else value = speeds[0] + state.fieldF8C * (speeds[1] - speeds[0]);
    if (state.field1130 > 0) value *= speeds[6];
    if (state.field1338 == 2) value *= speeds[7];
    if (state.flags10E8[1]) value *= state.field1108;
    if (state.field1170 != 0) value *= speeds[8];
    const auto& reactions = *player.components->reactions;
    const auto* record = reactions.entries.at(reactions.state);
    if (!record) throw std::runtime_error("Player action speed requires its original reaction record.");
    float multiplier;
    std::memcpy(&multiplier, &(*record)[5], 4);
    return value * multiplier;
}

playeractions::Input playeractions::Context::build(playeractor::Player& player, const ContextSignals& signals) const {
    auto& state = player.state;
    const auto& controls = *player.components->controls;
    const auto& actions = *player.components->actions;
    const auto reaction = player.components->reactions->state;
    Input result;
    player.flags &= ~0x1000u;
    result.flags[0] = player.fieldA0 >= 0 || player.fieldB2 >= 0 || signals.ladder;
    result.flags[1] = reaction >= 1 && reaction <= 3 && (state.flags792 & 12) && state.field1180 != 0;
    result.flags[2] = !state.flags1140[2] && state.field1180 == 5 && !state.flagsFC0[0];
    result.flags[3] = controls.flags30[0] && state.field1180 >= 2 && state.field1180 <= 4;
    result.flags[4] = state.flags1040[0];
    result.flags[5] = (state.flags792 & 8) && state.field7AC;
    result.flags[6] = state.flags1140[2];
    result.direction = state.direction;
    result.velocity = player.velocity;
    result.frame = player.animationFrame;
    result.thresholds = {thresholds[0], thresholds[1]};
    result.maximumSpeed = maximumSpeed(player);
    result.motion = state.fieldFB8 ? state.fieldFB8->value : 0;
    if (!state.field11A0) {
        if (state.field1070 == 3 && actions.field310 == 1 && (!state.fieldFB8 || state.fieldFB8->value == 0)) {
            result.modes[0] = 2;
            if (!state.fieldFBC) state.fieldF68 = 0;
            else {
                result.turn = state.fieldFBC->second[0] * state.fieldFBC->value;
                if (reaction != 4 && !state.flags134C[1] && state.field1348 == 0 && state.field1338 == 1 && state.field1130 == 0)
                    state.fieldF68 *= 1 + turnFactor * state.fieldFBC->value;
            }
        } else if (state.field1070 == 2) {
            const auto matrix = playerorientation::toMatrix(player.components->quaternion->quaternion);
            float x = matrix[0] * state.direction[0] + matrix[4] * state.direction[1] + matrix[8] * state.direction[2];
            const float z = matrix[2] * state.direction[0] + matrix[6] * state.direction[1] + matrix[10] * state.direction[2];
            const float length = std::sqrt(x * x + z * z);
            x = length >= 0.0001f ? x * (1 / length) : 0;
            result.modes[0] = 2;
            result.turn = state.fieldFB8 ? x * state.fieldFB8->value : 0;
            state.fieldF68 *= 1 + turnFactor * result.turn;
        } else result.modes[0] = !result.flags[1] && !state.flagsFC0[0] ? 1 : 3;
    } else {
        result.modes[0] = 4;
        result.modes[2] = state.field11A0->flags & 1 ? 2 : state.field11A0->flags & 4 ? 3 : 1;
    }
    if (state.field1130 > 0 && state.field1138 > 0) state.fieldF68 *= reactionFactor;
    result.modes[1] = reaction;
    result.modes[3] = 1;
    result.useMotion = (signals.ladder && !state.field10B0) || state.flags134C[1] || state.flags134C[2];
    result.animationFlags = {std::uint8_t((player.flags & 0x20000) != 0), std::uint8_t((player.flags & 0x40000) != 0),
        std::uint8_t(signals.footTiming), std::uint8_t(signals.footCrossing > 0),
        std::uint8_t(state.fieldFB8 && thresholds[1] <= state.fieldFB8->value),
        std::uint8_t(state.fieldFB8 && thresholds[2] <= state.fieldFB8->value), std::uint8_t(!state.flags1330[2])};
    if (player.life == 0) result.mode = 4;
    else if (state.flags134C[0]) result.mode = 9;
    else if (state.field1348 > 0) result.mode = 14;
    else if (state.field1130 > 0) result.mode = state.field1134 == 1 ? 12 : 11;
    else if (state.field1338 == 2) result.mode = 10;
    else if (state.field1338 == 4) result.mode = 13;
    else if (state.field11A0 && (state.field11A0->flags & 8)) result.mode = 2;
    else if (state.field1114 > 0) result.mode = 5;
    else if (state.field110C > 0) result.mode = 6;
    else {
        const auto threshold = idleThresholds[state.flags1040[0] ? 2 : state.flags134C[1] ? 1 : state.flags792 & 8 ? 0 : 3];
        result.mode = state.field116C <= float(threshold) ? 0 : state.flags1040[0] ? 7 : state.flags134C[1] ? 8 : 6;
    }
    result.actor = &player;
    result.attached = player.components->attached.get();
    result.controller = state.field7A0;
    return result;
}

playeractions::Input playeractions::Context::build(playeractor::Player& player, const mapcollision::State& collision,
    const actormodel::Catalog& catalog, const actormodel::Resources& models) const {
    const auto& type = catalog.type(player.type());
    const auto key = type.animations.at(player.animationState)[0];
    const auto timing = models.timing(type.model, key);
    const double previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
    return build(player, {collision.ladderContact(static_cast<std::uint16_t>(player.state.field10E0), player.position),
        actoranimation::sampleTiming(timing.get(), "Lfoot_timing", player.animationFrame) != -10000,
        actoranimation::timingCrossing(timing.get(), "Lfoot_timing", previous, player.animationFrame)});
}
