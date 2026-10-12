#include "game/player/playerdoorapproach.h"
#include "game/player/playerorientationupdate.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using playerinteraction::Vector;

    float normalize(Vector& value, float epsilon) {
        const float magnitude = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (magnitude >= epsilon) {
            const float inverse = 1 / magnitude;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
        return magnitude;
    }
}

playerinteraction::DoorApproach::DoorApproach(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::playerDoorApproachConstants, 40, bytes, error)) throw std::runtime_error(error);
    const auto read = [&](unsigned index) {
        const unsigned offset = index * 4;
        const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    epsilon = read(3);
    turnDistance = read(4);
    frontOffset = read(6);
    backOffset = read(7);
    lateralOffset = read(8);
    axisThreshold = read(9);
}

void playerinteraction::DoorApproach::locate(playercomponents::DoorState& state, const Vector& position,
    const DoorGeometry& geometry) const {
    for (unsigned axis : {0u, 2u}) {
        if (state.side == 1) state.approachPosition[axis] = geometry.position[axis] + frontOffset * geometry.normal[axis];
        else if (state.side == 0) state.approachPosition[axis] = geometry.position[axis] + backOffset * geometry.normal[axis] +
            lateralOffset * geometry.tangent[axis];
    }
    state.approachPosition[1] = position[1];
}

playerinteraction::Vector playerinteraction::DoorApproach::facing(const Vector& position, const DoorGeometry& geometry) const {
    Vector result{};
    const unsigned axis = std::abs(geometry.normal[2]) <= axisThreshold ? 0 : 2;
    result[axis] = geometry.position[axis] <= position[axis] ? -1.0f : 1.0f;
    return result;
}

playerinteraction::DoorMotion playerinteraction::DoorApproach::begin(playercomponents::DoorState& state,
    playeractions::Actions& actions, const Vector& position, std::uint32_t room, unsigned reaction,
    bool primary, std::int16_t pairedDoor) const {
    state.pairedDoor = state.door < 0 ? -1 : pairedDoor;
    state.sourceRoom = room;
    std::array<std::uint32_t, 12> payload{};
    payload[6] = reaction;
    if (state.side == 1) payload[8] = 1;
    else if (state.side == 0) payload[8] = 2;
    if (state.door < 0) {
        payload[9] = 3;
        state.command = 1;
    } else if (!primary) {
        payload[9] = 2;
        state.command = 3;
    } else {
        payload[9] = 1;
        state.command = 1;
    }
    actions.request(12, &payload);
    DoorMotion motion;
    for (unsigned axis = 0; axis < 3; ++axis) motion.facing[axis] = state.approachPosition[axis] - position[axis];
    if (normalize(motion.facing, epsilon) == 0) motion.facing = state.facing;
    state.approachFrames = 0;
    return motion;
}

playerinteraction::ApproachResult playerinteraction::DoorApproach::advance(playercomponents::DoorState& state,
    DoorMotion& motion, const Vector& position, float speed, bool contact) const {
    Vector direction{state.approachPosition[0] - position[0], 0, state.approachPosition[2] - position[2]};
    const float distance = normalize(direction, epsilon);
    ++state.approachFrames;
    if (distance < speed || state.approachFrames > 300) {
        for (unsigned axis = 0; axis < 3; ++axis) motion.displacement[axis] = state.approachPosition[axis] - position[axis];
        motion.facing = state.facing;
        return ApproachResult::Arrived;
    }
    if (contact) return ApproachResult::Blocked;
    for (unsigned axis = 0; axis < 3; ++axis) motion.displacement[axis] = direction[axis] * speed;
    float blend = distance / (turnDistance * speed);
    if (blend < 0) blend = 0;
    if (blend > 1) blend = 1;
    motion.facing = playerorientation::direction(playerorientation::interpolate(blend,
        playerorientation::heading(state.facing, false), playerorientation::heading(direction, false)));
    return ApproachResult::Moving;
}
