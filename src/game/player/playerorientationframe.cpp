#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/player/playerorientationframe.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    enum Parameter : unsigned {
        reverseProjectionThreshold = 1,
        movementThreshold = 2,
        movementUpperThreshold = 3,
        turnLimit = 7,
        maximumSpeed = 8,
        alternateMaximumSpeed = 9,
        acceleration = 10,
        primaryStateAcceleration = 11,
        equipmentAcceleration = 12,
        scannerAcceleration = 13,
        secondaryStateAcceleration = 14,
        primaryStateDamping = 15,
        secondaryStateDamping = 16,
        defaultBlend = 29,
        targetSpeedMultiplier = 32
    };

    void read(const std::vector<std::uint8_t>& executable, unsigned address, float* destination, unsigned count) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, count * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < count; ++i) {
            const auto offset = i * 4;
            const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
                (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
            std::memcpy(destination + i, &bits, 4);
        }
    }
}

playerorientation::Frame::Frame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose) : pose(pose) {
    read(executable, glme01::playerTurningParameters, constants.data(), static_cast<unsigned>(constants.size()));
    read(executable, glme01::playerTurnCompletionThreshold, &completion, 1);
}

playerorientation::Input playerorientation::Frame::build(playeractor::Player& player, const FrameSignals& signals) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto& reactions = *player.components->reactions;
    const auto settings = player.components->controls->settings.flags;
    const bool ground = player.fieldA0 >= 0 || player.fieldB2 >= 0;
    const bool grounded = ground && !(actions.field310 == 2 && (actions.field314 == 0x20 || actions.field314 == 0x21));
    const bool canTurn = (actions.field310 == 1 && actions.fields31C[4] != 6) ||
        (actions.field310 == 2 && actions.field314 == 0xc);
    if (!ground && !(actions.field310 == 2 && (actions.field314 == 0x20 || actions.field314 == 0x21)) && state.field1070 < 4) {
        Vector direction{state.direction[0], 0, state.direction[2]};
        const float magnitude = std::sqrt(direction[0] * direction[0] + direction[2] * direction[2]);
        if (magnitude >= 0.0001f) {
            for (auto& value : direction) value *= 1.0f / magnitude;
        } else direction.fill(0);
        if (magnitude != 0) {
            state.field1070 = 7;
            state.field1090 = direction;
            state.field109C = constants[defaultBlend];
        }
    }
    Input result;
    const auto* movement = state.fieldFB8;
    const auto* turning = state.fieldFBC;
    if (state.field1070 < 4) {
        result.blend = state.flags1140[2] && grounded && actions.field310 == 1 ? 1 : constants[defaultBlend];
        if ((signals.attachedTurn && state.field10A0 <= 0) || state.flags134C[0] ||
            (movement && movement->value == 0 && !actions.query74588() && !state.flagsFC0[0] &&
             (reactions.state != 4 || (turning && turning->value != 0)) && (settings & 0x20))) state.field1070 = 3;
        else {
            float projection = 0;
            if (movement) for (unsigned i = 0; i < 3; ++i) projection += movement->second[i] * state.direction[i];
            state.field1070 = movement && movement->value >= constants[movementThreshold] && movement->value < constants[movementUpperThreshold] &&
                !state.flagsFC0[0] && projection < constants[reverseProjectionThreshold] ? 2 : 1;
        }
    } else if (state.field1070 < 8) result.blend = state.field109C;
    if (state.flags1140[2]) {
        result.acceleration = constants[scannerAcceleration];
        result.damping = signals.scannerDamping;
    } else if (state.flags134C[0]) {
        result.acceleration = constants[primaryStateAcceleration];
        result.damping = constants[primaryStateDamping];
    } else if (state.flags134C[1]) {
        result.acceleration = constants[secondaryStateAcceleration];
        result.damping = constants[secondaryStateDamping];
    } else {
        result.acceleration = state.field1180 == 5 && !(settings & 0x40) ? constants[equipmentAcceleration] : constants[acceleration];
        result.damping = signals.damping;
    }
    result.maximumSpeed = constants[state.flags134C[1] ? alternateMaximumSpeed : maximumSpeed];
    if (state.field11A0 && (state.field11A0->flags & 0x4000)) result.maximumSpeed *= constants[targetSpeedMultiplier];
    result.turnLimit = constants[turnLimit];
    result.position = player.position;
    result.velocity = player.velocity;
    result.target = {state.field1080[0], state.field1080[1], state.field1080[2], state.field108C};
    result.targetPosition = state.field1074;
    result.targetDirection = state.field1090;
    result.mode = state.field1070;
    result.movement = movement;
    result.turning = turning;
    result.threshold = constants[movementThreshold];
    result.enabled = canTurn && grounded && (!state.flagsFC0[0] || result.mode == 3);
    const auto* reaction = reactions.entries.at(reactions.state);
    if (!reaction) throw std::runtime_error("Player orientation requires its retained reaction parameters.");
    result.reverse = ((*reaction)[6] >> 24) != 0;
    result.inverted = state.flags1330[1] != 0;
    return result;
}

void playerorientation::Frame::run(playeractor::Player& player, const FrameSignals& signals) const {
    auto& state = player.state;
    const auto input = build(player, signals);
    const float alignment = update(*player.components->quaternion, state.quaternion, state.quaternion, input, pose);
    state.direction = direction(state.quaternion);
    auto rotation = state.quaternion;
    if (state.field10C8 > 0) {
        const float degrees = state.field10C8 * (signals.frame & 1 ? -1.0f : 1.0f);
        const auto turn = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<std::int32_t>((65536.0f * (0.5f * degrees)) / 360.0f)));
        rotation = multiply(rotation, {0, 0, turn[0], turn[2]});
    }
    player.rotation = pose.angles(toMatrix(rotation));
    player.previousRotation = player.rotation;
    if (state.field1070 > 3 && state.field1070 < 8 && alignment > completion) state.field1070 = 0;
}
