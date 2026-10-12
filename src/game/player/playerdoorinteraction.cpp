#include "game/player/playerdoorinteraction.h"
#include "game/player/playerorientationupdate.h"
#include "game/actors/actorpathsampler.h"
#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> read(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned index) {
        const unsigned offset = index * 4;
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    float scalar(const std::vector<std::uint8_t>& bytes, unsigned index) {
        const auto bits = word(bytes, index);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    playerinteraction::DoorGeometry geometry(const doors::CollisionBox& box) {
        return {box.center, box.axes[0], box.axes[2]};
    }
}

playerinteraction::DoorInteraction::DoorInteraction(const std::vector<std::uint8_t>& executable) :
    approach(executable), fallback(executable, 0) {
    const auto selection = read(executable, glme01::playerDoorSelectionConstants, 8);
    padding = scalar(selection, 0);
    facingThreshold = scalar(selection, 1);
    const auto types = read(executable, glme01::fakeDoorActorTypes, 12);
    const auto box = read(executable, glme01::doorCollisionBoxes, 24);
    for (unsigned i = 0; i < 3; ++i) {
        fakeTypes[i] = word(types, i);
        fakeOrigin[i] = scalar(box, i);
        fakeSize[i] = scalar(box, i + 3);
    }
}

bool playerinteraction::DoorInteraction::contains(const doors::CollisionBox& box, const Vector& position, float padding) {
    Vector relative;
    for (unsigned i = 0; i < 3; ++i) relative[i] = position[i] - box.center[i];
    for (unsigned i = 0; i < 3; ++i) {
        const auto& axis = box.axes[i];
        const float projected = (relative[2] * axis[2] + relative[0] * axis[0]) + relative[1] * axis[1];
        if (!(projected < box.extents[i] + padding && -padding < projected)) return false;
    }
    return true;
}

bool playerinteraction::DoorInteraction::select(playercomponents::DoorState& state, const DoorInput& input,
    DoorServices& services) const {
    const auto accepts = [&](const doors::CollisionBox& box, Vector& facing) {
        if (!contains(box, input.position, input.radius + padding)) return false;
        facing = approach.facing(input.position, geometry(box));
        return (facing[0] * input.direction[0] + facing[1] * input.direction[1]) +
            facing[2] * input.direction[2] >= facingThreshold;
    };
    for (unsigned i = 0; i < services.doors.size(); ++i) {
        const auto& leaf = services.doors.leaf(i);
        if (!leaf.collisionActive || !leaf.collision || (services.doors.action(i, 0, 0, false) & 0x10fc) != 8) continue;
        Vector facing;
        if (!accepts(*leaf.collision, facing)) continue;
        state.door = static_cast<std::int16_t>(i);
        state.side = services.doors.entrySide(i, input.room);
        approach.locate(state, input.position, geometry(*leaf.collision));
        state.facing = facing;
        return true;
    }
    for (const auto& slot : services.enemies.slots()) {
        if (slot.state != 2 || !slot.owner) continue;
        auto* actor = dynamic_cast<actors::MoveActor*>(&slot.owner->actor());
        if (!actor || actor->room() != input.room || std::find(fakeTypes.begin(), fakeTypes.end(), actor->type()) == fakeTypes.end()) continue;
        auto matrix = actor->matrix;
        if (actor->model) matrix = dooranimation::concatenate(matrix, actor->model->matrix(0, 0));
        doors::CollisionBox box;
        for (unsigned row = 0; row < 3; ++row) {
            const float first = matrix[row * 4] * fakeOrigin[0], second = matrix[row * 4 + 1] * fakeOrigin[1];
            box.center[row] = std::fma(matrix[row * 4 + 2], fakeOrigin[2], first) + std::fma(matrix[row * 4 + 3], 1.0f, second);
            for (unsigned axis = 0; axis < 3; ++axis) box.axes[axis][row] = matrix[row * 4 + 2 - axis];
            box.extents[row] = fakeSize[2 - row];
        }
        Vector facing;
        if (!accepts(box, facing)) continue;
        state.door = -1;
        state.side = 1;
        approach.locate(state, input.position, geometry(box));
        state.facing = facing;
        state.fakeDoor = actor;
        return true;
    }
    return false;
}

void playerinteraction::DoorInteraction::path(const playercomponents::DoorState& state, const playeractions::Actions& actions,
    const DoorInput& input, DoorOutput& output, const DoorServices& services) const {
    output.motion.facing = state.facing;
    Vector displacement;
    const auto sample = [&](unsigned type, unsigned animation) {
        const auto& info = services.catalog.type(type);
        const auto& selected = info.animations.at(animation);
        return actoranimation::pathDisplacement(services.models, info.model, selected[0], input.previousFrame,
            input.frame, (selected[5] & 2) != 0, displacement);
    };
    if (!sample(input.actorType, input.animation) && !sample(0, fallback.entries.at(actions.field60))) return;
    const auto rotation = playerorientation::toMatrix(playerorientation::heading(state.facing, false));
    for (unsigned i = 0; i < 3; ++i) output.motion.displacement[i] = rotation[i * 4] * displacement[0] +
        rotation[i * 4 + 1] * displacement[1] + rotation[i * 4 + 2] * displacement[2];
}

void playerinteraction::DoorInteraction::open(playercomponents::DoorState& state, playeractions::Actions& actions,
    const DoorInput& input, DoorOutput& output, DoorServices& services) const {
    if (state.door < 0) {
        if (!state.fakeDoor) throw std::runtime_error("Fake-door interaction requires its registered actor.");
        state.fakeDoor->selectAnimation("OPEN");
        actions.request(0x15);
        output.damage = 10;
    } else if (!state.unlocked) {
        actions.request(0x16);
        output.blocked = state.blocked != 0;
    } else {
        services.doors.action(state.door, state.side, state.command, false);
        if (state.pairedDoor == -1) services.doors.destinationRoom(state.door, input.room, services.roomIds, output.room);
        actions.request(0x15);
    }
}

void playerinteraction::DoorInteraction::arrive(playercomponents::DoorState& state, playeractions::Actions& actions,
    const DoorInput& input, DoorOutput& output, DoorServices& services) const {
    state.unlocked = 1;
    state.blocked = 0;
    bool present = false;
    std::uint8_t savedId = 0, type = 0;
    if (state.door >= 0) {
        if (services.doors.eventBlocked(state.door & 255)) {
            state.unlocked = 0;
            state.blocked = 1;
        } else if (services.doors.markUsed(state.door, savedId, type)) {
            if (!(services.keys[savedId >> 3] & (1u << (savedId & 7)))) state.unlocked = 0;
            else services.doors.setSavedFlag(savedId, false, services.savedDoors);
            present = (state.flags & 1) != 0;
        }
    }
    if (present) {
        if (!services.presentKey) throw std::runtime_error("Door key presentation requires its original scene service.");
        services.presentKey(!state.unlocked, state.side == 1, type);
        actions.request(0x14);
    } else open(state, actions, input, output, services);
}

bool playerinteraction::DoorInteraction::run(playercomponents::DoorState& state, playeractions::Actions& actions,
    const DoorInput& input, DoorOutput& output, DoorServices& services) const {
    DoorOutput next;
    next.motion.facing = state.facing;
    next.room = input.room;
    if (actions.field310 == 2 && actions.field314 == 0x20) {
        switch (actions.fields31C[0]) {
        case 1: {
            const auto result = approach.advance(state, next.motion, input.position, input.speed, input.contact);
            if (result == ApproachResult::Blocked) actions.request(0x13);
            else if (result == ApproachResult::Arrived) arrive(state, actions, input, next, services);
            break;
        }
        case 2:
            if (!services.keyPresentationActive) open(state, actions, input, next, services);
            break;
        case 3:
            path(state, actions, input, next, services);
            if (input.animationEnded && state.pairedDoor != -1) {
                state.door = state.pairedDoor;
                const auto& box = services.doors.leaf(state.door).collision;
                if (!box) throw std::runtime_error("Paired door requires its original collision geometry.");
                approach.locate(state, input.position, geometry(*box));
                state.pairedDoor = -1;
                state.unlocked = 1;
                next.position = &state.approachPosition;
                open(state, actions, input, next, services);
            }
            break;
        case 5:
            if (input.close) {
                services.doors.action(state.door, state.side, 4, false);
                actions.request(0x17);
            } else if (input.reverse) {
                services.doors.action(state.door, state.side, 2, false);
                if (state.pairedDoor == -1) services.doors.destinationRoom(state.door, input.room, services.roomIds, next.room);
                actions.request(0x18);
            }
            break;
        case 8:
            path(state, actions, input, next, services);
            next.room = state.sourceRoom;
            break;
        case 9:
            path(state, actions, input, next, services);
            for (unsigned i = 0; i < 3; ++i) next.motion.facing[i] = -state.facing[i];
            next.instantTurn = next.reverseFacing = true;
            break;
        case 10:
        case 11:
            for (unsigned i = 0; i < 3; ++i) next.motion.facing[i] = -state.facing[i];
            next.reverseFacing = true;
            if (actions.fields31C[0] == 10 && input.animationLooped) actions.request(0x19);
            break;
        default:
            path(state, actions, input, next, services);
            break;
        }
    } else if (actions.field310 == 1 && input.eligible && input.pressed && select(state, input, services)) {
        const auto paired = state.door < 0 ? std::int16_t(-1) : services.doors.pairedEntry(state.door, input.room, services.pairingDisabled);
        next.motion = approach.begin(state, actions, input.position, input.room, input.reaction, input.primary, paired);
    } else return false;
    output = next;
    return true;
}
