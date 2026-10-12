#include "game/player/playerdoorframe.h"
#include "game/events/runtimeflags.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

playerinteraction::DoorFrame::DoorFrame(const std::vector<std::uint8_t>& executable) :
    interaction(executable), actions(executable), damage(executable),
    approachSpeed(scalar(executable, glme01::playerDoorApproachSpeed)),
    turnBlend(scalar(executable, glme01::playerDoorTurnBlend)), retryDelay(word(executable, glme01::playerDoorRetryDelay)) {}

playerinteraction::DoorInput playerinteraction::DoorFrame::input(const playeractor::Player& player) const {
    const auto& state = player.state;
    const auto& controls = *player.components->controls;
    DoorInput result;
    result.position = player.position;
    result.direction = state.field1180 == 0 ? state.direction : player.components->weapon->first->field610;
    result.speed = approachSpeed * actions.maximumSpeed(player);
    result.actorType = player.actorType;
    result.radius = state.fieldF54;
    result.reaction = player.components->reactions->state;
    result.animation = player.animationState;
    result.frame = player.animationFrame;
    result.previousFrame = player.animationFrame - player.animationFrameStep;
    result.room = player.fieldB4;
    result.eligible = (state.flags792 & 1) && state.field1130 == 0 && state.field1338 == 1 &&
        state.field1180 != 1 && state.field1180 != 6 && (state.field1184 == 1 || state.field1180 != state.field1184) &&
        !state.flags1140[2] && !state.flags1330[1] && !state.field1060 && state.field10E4 == 0 && state.field1170 == 0 &&
        (result.reaction == 1 || result.reaction == 2 || result.reaction == 3);
    result.pressed = controls.flagsC[6] != 0;
    result.close = controls.flagsC[8] != 0;
    result.reverse = controls.flagsC[9] != 0;
    result.primary = controls.flagsC[7] != 0;
    result.animationEnded = (player.flags & 0x40000) != 0;
    result.animationLooped = (player.flags & 0x20000) != 0;
    result.contact = player.field9C >= 0;
    return result;
}

bool playerinteraction::DoorFrame::run(playeractor::Player& player, DoorFrameServices& services) const {
    const auto next = input(player);
    auto& state = player.state;
    state.flags10E8[0] = 0;
    if (static_cast<std::int32_t>(state.field10E4) > 0) --state.field10E4;
    DoorOutput output;
    if (!interaction.run(*player.components->door, *player.components->actions, next, output, services.interaction)) return false;
    state.fieldF70 = 1;
    state.fieldF74 = output.motion.displacement;
    state.field1070 = 7;
    state.field1090 = output.motion.facing;
    state.field1090[1] = 0;
    const float length = std::sqrt((state.field1090[0] * state.field1090[0] + state.field1090[1] * state.field1090[1]) +
        state.field1090[2] * state.field1090[2]);
    if (length >= 0.0001f) for (auto& component : state.field1090) component *= 1 / length;
    else state.field1090.fill(0);
    state.field109C = output.instantTurn ? 1 : turnBlend;
    if (output.position) {
        player.setPosition(*output.position);
        if (!services.globalSound) throw std::runtime_error("Paired-door relocation requires original global sound playback.");
        services.globalSound(0x8000001c);
    }
    if (output.room != player.fieldB4) {
        player.fieldB4 = output.room;
        for (unsigned flag = 0x55; flag < 0xaa; ++flag) services.flags.setOff(static_cast<std::uint8_t>(flag));
    }
    state.flags10E8[0] = !output.reverseFacing;
    state.field10E4 = retryDelay;
    if (static_cast<std::int32_t>(output.damage) > 0) state.field112C = output.damage;
    if (output.blocked) {
        playerevents::Payload payload;
        payload.word(0x18, 5);
        payload.word(0x20, 3);
        damage.run(player, payload, false, services.actors, services.interaction.catalog, services.doubledDamage, services.life);
    }
    return true;
}
