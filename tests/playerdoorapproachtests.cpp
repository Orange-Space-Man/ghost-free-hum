#include "game/player/playerdoorapproach.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> bytes(0x2e0100);
        const auto word = [&](unsigned offset, unsigned value) {
            for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        word(0, 0x100);
        word(0x48, 0x80200000);
        word(0x90, 0x2e0000);
        const float constants[]{0, 1, -1, 0.0001f, 4, 0, 30, -20, 8, 0.5f};
        for (unsigned i = 0; i < 10; ++i) {
            unsigned bits;
            std::memcpy(&bits, &constants[i], 4);
            word(0x2daa60 + i * 4, bits);
        }
        return bytes;
    }

    void check() {
        const auto executable = fixture();
        playerinteraction::DoorApproach approach(executable);
        playercomponents::DoorState state;
        const playerinteraction::DoorGeometry geometry{{100, 200, 300}, {0, 0, 1}, {1, 0, 0}};
        approach.locate(state, {0, 12, 0}, geometry);
        require(state.approachPosition == playerinteraction::Vector{100, 12, 330}, "Front approach preserves player height.");
        state.side = 0;
        approach.locate(state, {0, 18, 0}, geometry);
        require(state.approachPosition == playerinteraction::Vector{108, 18, 280}, "Back approach includes the original lateral offset.");
        state.side = 2;
        approach.locate(state, {0, 7, 0}, geometry);
        require(state.approachPosition == playerinteraction::Vector{108, 7, 280}, "Unrecognized sides retain horizontal coordinates.");
        require(approach.facing({0, 0, 300}, geometry) == playerinteraction::Vector{0, 0, -1}, "Equality faces the negative door axis.");
        auto xDoor = geometry;
        xDoor.normal[2] = -0.5f;
        require(approach.facing({90, 0, 300}, xDoor) == playerinteraction::Vector{1, 0, 0}, "The axis threshold is inclusive and uses absolute normal.");

        playeractions::Actions actions(executable, 0);
        state.side = 1;
        state.approachPosition = {0, 4, 0};
        state.facing = {0, 0, -1};
        state.approachFrames = 90;
        auto motion = approach.begin(state, actions, state.approachPosition, 0x02000102, 3, true, 7);
        require(state.pairedDoor == 7 && state.sourceRoom == 0x02000102 && state.approachFrames == 0,
            "Beginning approach retains its room and paired leaf and resets its frame counter.");
        require(motion.displacement == playerinteraction::Vector{} && motion.facing == state.facing,
            "Coincident approach targets preserve the selected door facing.");
        require(actions.field310 == 2 && actions.field314 == 32 && actions.field358 == 3 &&
            actions.fields31C[11] == 1 && actions.fields31C[12] == 1, "Door entry reaches the real protected action dispatcher.");

        actions.field310 = 1;
        state.door = -1;
        approach.begin(state, actions, {0, 4, -10}, 9, 2, false, 5);
        require(state.pairedDoor == -1 && state.command == 1 && actions.fields31C[12] == 3,
            "Fake doors use their original reaction and never retain a paired leaf.");
        actions.field310 = 1;
        state.door = 2;
        state.side = 0;
        approach.begin(state, actions, {0, 4, -10}, 9, 2, false, 5);
        require(state.command == 3 && actions.fields31C[11] == 2 && actions.fields31C[12] == 2,
            "Secondary entry preserves the side and alternate opening command.");

        state.approachPosition = {0, 4, 10};
        state.facing = {1, 0, 0};
        state.approachFrames = 0;
        motion = {};
        require(approach.advance(state, motion, {0, 1, 0}, 2, false) == playerinteraction::ApproachResult::Moving &&
            motion.displacement == playerinteraction::Vector{0, 0, 2}, "Approach motion uses horizontal distance and original per-frame speed.");
        require(std::abs(motion.facing[2] - 1) < 0.0001f, "Distant approach faces its movement direction.");
        const auto previous = motion;
        require(approach.advance(state, motion, {0, 1, 0}, 2, true) == playerinteraction::ApproachResult::Blocked &&
            motion.displacement == previous.displacement && motion.facing == previous.facing, "Blocked approach leaves the output unchanged.");
        require(approach.advance(state, motion, {0, 1, 8}, 2, false) == playerinteraction::ApproachResult::Moving,
            "Distance equal to speed is not arrival.");
        require(std::abs(std::sqrt(motion.facing[0] * motion.facing[0] + motion.facing[2] * motion.facing[2]) - 1) < 0.0001f &&
            motion.facing[0] > motion.facing[2] && motion.facing[2] > 0, "Near-door facing uses normalized quaternion interpolation.");
        require(approach.advance(state, motion, {0, 1, 9}, 2, true) == playerinteraction::ApproachResult::Arrived &&
            motion.displacement == playerinteraction::Vector{0, 3, 1}, "Arrival takes precedence over contact and retains vertical displacement.");
        state.approachFrames = 299;
        require(approach.advance(state, motion, {0, 1, 0}, 2, false) == playerinteraction::ApproachResult::Moving,
            "Frame 300 still approaches.");
        require(approach.advance(state, motion, {0, 1, 0}, 2, true) == playerinteraction::ApproachResult::Arrived &&
            motion.displacement == playerinteraction::Vector{0, 3, 10}, "Frame 301 forces the original full displacement even on contact.");
    }
}

int main() {
    try {
        check();
        std::cout << "Original door approach checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
