#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/player/playervelocitycontext.h"
#include "game/startup/startupresources.h"

#include <cstring>
#include <stdexcept>

namespace {
    float number(std::uint32_t value) {
        float result;
        std::memcpy(&result, &value, 4);
        return result;
    }
}

playermotion::VelocityContext::VelocityContext(const std::vector<std::uint8_t>& executable) : actions(executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::playerTurningParameters, constants.size() * 4, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < constants.size(); ++i) {
        const unsigned offset = i * 4;
        constants[i] = number((unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3]);
    }
}

playermotion::VelocityInput playermotion::VelocityContext::build(const playeractor::Player& player, const mapcollision::State& collision) const {
    VelocityInput result;
    const auto& state = player.state;
    const auto& action = *player.components->actions;
    const auto& reactions = *player.components->reactions;
    result.motion = state.fieldFB8;
    result.direction = &state.direction;
    result.position = player.position;
    result.floor = player.fieldA0 >= 0 ? 0xfffe : player.fieldB2 >= 0 ? static_cast<std::uint16_t>(player.fieldB2) : 0xffff;
    result.wall = static_cast<std::uint16_t>(player.fieldB0);
    result.probe = static_cast<std::uint16_t>(state.field10E0);
    result.drag = constants[0];
    if (state.flags134C[0]) {
        result.idleFriction = result.movingFriction = constants[19];
        result.acceleration = constants[24];
        result.reflection = constants[27];
        result.ladderGravity = 0;
    } else if (state.flags134C[1]) {
        result.idleFriction = result.movingFriction = constants[20];
        result.acceleration = constants[25];
        result.reflection = constants[28];
        result.ladderGravity = constants[6];
    } else {
        result.idleFriction = constants[17];
        result.movingFriction = constants[18];
        result.acceleration = state.field1180 == 1 || state.field1180 == 6 ? constants[23] :
            constants[21] + state.fieldF8C * (constants[22] - constants[21]);
        result.reflection = constants[26];
        result.ladderGravity = constants[6];
    }
    const auto* reaction = reactions.entries.at(reactions.state);
    if (!reaction) throw std::runtime_error("Player movement requires its retained reaction parameters.");
    result.directionalForce = result.acceleration * number((*reaction)[4]);
    result.acceleration *= number((*reaction)[3]);
    result.reversal = state.flags134C[1] || player.fieldB0 >= 0 ? 1.0f : 3.0f;
    result.gravity = constants[5];
    result.threshold = state.flagsFC0[0] ? constants[2] : constants[3];
    result.maximumSpeed = actions.maximumSpeed(player);
    const bool protectedReaction = action.field310 == 2 && (action.field314 == 0x20 || action.field314 == 0x21 || action.field314 == 0x1f);
    result.applyFriction = result.applyGravity = result.applyLadderGravity = !protectedReaction;
    result.applyReflection = true;
    result.inverted = state.flags1330[1] != 0;
    result.applyDirection = action.field310 == 1;
    result.emitSound = state.flags134C[0] != 0;
    if (state.field106C == 0 && action.field310 == 1 && action.fields31C[4] != 6 &&
        (result.floor != 0xffff || collision.ladderContact(result.probe, player.position))) {
        if (state.flagsFC0[0] || !result.motion || result.motion->value < constants[2] || result.motion->value >= constants[3])
            result.applyMotion = true;
        else {
            const auto& direction = result.motion->second;
            const float projection = direction[0] * state.direction[0] + direction[1] * state.direction[1] + direction[2] * state.direction[2];
            if (projection >= constants[1]) {
                result.applyMotion = true;
                result.threshold = constants[2];
            }
        }
    }
    return result;
}
