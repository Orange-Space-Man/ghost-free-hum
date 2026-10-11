#include "resources/dolresources.h"
#include "game/player/playervelocityframe.h"
#include "game/player/playerorientationupdate.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float read(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    float length(const playermotion::Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }
}

playermotion::VelocityFrame::VelocityFrame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose) :
    pose(pose), context(executable), actions(executable), scriptedSpeed(read(executable, 0x804db570)),
    scriptedBlend(read(executable, 0x804d2ba4)), radialForce(read(executable, 0x804d2b84)),
    radialTurnDistance(read(executable, 0x804db58c)) {}

void playermotion::VelocityFrame::constrain(playeractor::Player& player) const {
    auto& state = player.state;
    if (!state.flags134C[2]) return;
    Vector predicted, origin = state.field1350, direction;
    for (unsigned i = 0; i < 3; ++i) predicted[i] = player.position[i] + player.velocity[i];
    origin[1] = predicted[1];
    for (unsigned i = 0; i < 3; ++i) direction[i] = origin[i] - predicted[i];
    const float distance = length(direction);
    if (distance >= 0.0001f) for (auto& value : direction) value *= 1.0f / distance;
    else direction.fill(0);
    const auto addForce = [&](float magnitude) {
        Vector velocity;
        for (unsigned i = 0; i < 3; ++i) velocity[i] = direction[i] * magnitude + player.velocity[i];
        const float squared = (velocity[0] * velocity[0] + velocity[1] * velocity[1]) + velocity[2] * velocity[2];
        if (squared > 2500) {
            const float factor = 50.0f / std::sqrt(squared);
            for (auto& value : velocity) value *= factor;
        }
        player.setVelocity(velocity);
    };
    if (distance > state.field1360) {
        const float outer = state.field1360 + state.field1364;
        if (distance > outer) addForce(distance - outer);
        if (state.field1364 > 0) addForce((radialForce * (distance - state.field1360)) / state.field1364);
    }
    if (distance > radialTurnDistance) {
        state.field1070 = 5;
        state.field1074 = origin;
        state.field109C = scriptedBlend;
    }
}

void playermotion::VelocityFrame::run(playeractor::Player& player, const mapcollision::State& collision,
    const std::array<PlatformMotion, 2>& platforms, const std::function<void(std::uint32_t, const Vector&)>& sound) const {
    auto& state = player.state;
    state.flagsFC0[3] = 0;
    for (const auto& platform : platforms) {
        if (!(platform.radius > 0)) continue;
        Vector offset{platform.position[0] - player.position[0], 0, platform.position[2] - player.position[2]};
        if (!(length(offset) < platform.radius) || platform.rotation == 0) continue;
        const float degrees = (360.0f / 65536.0f) * platform.rotation;
        const auto turn = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<std::int32_t>((65536.0f * (0.5f * degrees)) / 360.0f)));
        playerorientation::Quaternion quaternion{0, turn[0], 0, turn[2]};
        const auto matrix = playerorientation::toMatrix(quaternion);
        Vector displacement;
        for (unsigned row = 0; row < 3; ++row) {
            displacement[row] = matrix[row * 4] * offset[0] + matrix[row * 4 + 1] * offset[1] + matrix[row * 4 + 2] * offset[2] - offset[row];
            player.position[row] += displacement[row];
        }
        player.field5C = player.position;
        const float norm = ((quaternion[3] * quaternion[3] + quaternion[2] * quaternion[2]) + quaternion[0] * quaternion[0]) +
            quaternion[1] * quaternion[1];
        if (norm > 0) {
            for (unsigned i = 0; i < 3; ++i) quaternion[i] *= -1.0f / norm;
            quaternion[3] *= 1.0f / norm;
        } else quaternion = {0, 0, 0, 1};
        auto& retained = player.components->quaternion->quaternion;
        retained = playerorientation::multiply(retained, quaternion);
        const float magnitude = std::sqrt(((retained[3] * retained[3] + retained[2] * retained[2]) + retained[0] * retained[0]) +
            retained[1] * retained[1]);
        if (magnitude != 0) for (auto& component : retained) component *= 1.0f / magnitude;
        state.flagsFC0[3] = 1;
        break;
    }
    if (state.field10B0) {
        state.fieldF74 = {state.field10B4[0] - player.position[0], 0, state.field10B4[2] - player.position[2]};
        const float distance = length(state.fieldF74);
        if (distance >= 0.0001f) for (auto& component : state.fieldF74) component *= 1.0f / distance;
        else state.fieldF74.fill(0);
        float speed;
        if (static_cast<std::int32_t>(state.field10C0) < 1) speed = scriptedSpeed * actions.maximumSpeed(player);
        else {
            speed = distance / float(static_cast<std::int32_t>(state.field10C0));
            --state.field10C0;
            if (static_cast<std::int32_t>(state.field10C0) < 1 || distance < scriptedSpeed * actions.maximumSpeed(player)) {
                state.field10C0 = 0;
                state.field10B0 = 0;
            }
        }
        state.field1070 = 7;
        state.field1090 = state.fieldF74;
        state.field109C = scriptedBlend;
        if (distance < speed) {
            state.field10B0 = 0;
            speed = distance;
        }
        for (auto& component : state.fieldF74) component *= speed;
        state.fieldF70 = 1;
    }
    if (!state.fieldF70) player.setVelocity(updateVelocity(player.velocity, context.build(player, collision), collision, sound));
    else {
        player.setVelocity(state.fieldF74);
        state.fieldF70 = 0;
        state.fieldF74.fill(0);
    }
}
