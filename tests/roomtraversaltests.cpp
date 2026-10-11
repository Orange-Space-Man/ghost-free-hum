#include "resources/glme01addresses.h"
#include "game/world/roomtraversalstate.h"
#include "game/world/roomvisibilitywalk.h"
#include "game/world/roomportalbounds.h"
#include "game/world/doordescriptors.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void word(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
}

int main() {
    try {
        std::vector<std::uint8_t> executable(512);
        word(executable, 0, 256);
        word(executable, 0x48, 0x80000000);
        word(executable, 0x90, 85);
        word(executable, 4, 400);
        word(executable, 0x4c, glme01::doorConfigurations);
        word(executable, 0x94, 60);
        executable[420] = 2;
        executable[440] = 7;
        for (unsigned i = 0; i < 3; ++i) {
            const unsigned offset = 256 + i * 28;
            executable[offset] = i == 1 ? 2 : 3;
            word(executable, offset + 4, i == 0 ? 0xff : (i * 256 + 255));
            word(executable, offset + 8, 0xfffffff9);
            executable[offset + 20] = 0xff;
            executable[offset + 21] = 0xfb;
            executable[offset + 26] = 4;
            executable[offset + 27] = 8;
        }
        doordata::Table doors;
        std::string error;
        require(doordata::read(executable, 0x80000000, doors, error), error.c_str());
        require(doors.entries.size() == 3 && doors.liveCount == 3,
            "Door allocation skips only exact 0xff, doubles type 2 and counts other configurations once.");
        require(doors.entries[0].center[0] == -7 && doors.entries[0].dimensions[0] == -5 &&
            doors.entries[0].rooms == std::array<std::uint8_t, 2>{4, 8}, "Door descriptors retain signed coordinates, dimensions and endpoints.");
        require(doordata::traversalEnabled(doors.entries[0]) && !doordata::traversalEnabled(doors.entries[1]),
            "Traversal checks bit zero independently of descriptor termination and allocation.");
        executable[340] = 1;
        require(!doordata::read(executable, 0x80000000, doors, error) && doors.entries.size() == 3,
            "Missing terminator cannot read outside the executable or replace valid descriptors.");
        const auto corners = roomportal::corners({10, 20, -30}, 5, 7);
        require(corners[0] == roomportal::Point{8, 17, -30} && corners[1] == roomportal::Point{12, 17, -30} &&
            corners[2] == roomportal::Point{12, 23, -30} && corners[3] == roomportal::Point{8, 23, -30},
            "Door corners preserve integer half-size truncation and original vertex order.");
        const auto reversed = roomportal::corners({0, 0, 0}, -5, -7);
        require(reversed[0] == roomportal::Point{2, 3, 0} && reversed[2] == roomportal::Point{-2, -3, 0},
            "Negative odd sizes divide toward zero rather than shifting down.");
        const auto wrapped = roomportal::corners({2147483647, 0, 0}, 2, 0);
        require(wrapped[1][0] == -2147483648.0f, "Corner integer arithmetic retains PowerPC word wrap.");
        roomportal::Matrix matrix{1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1};
        require(roomportal::transform(matrix, {2, 3, 4}) == std::array<float, 4>{12, 23, 34, 1},
            "Original matrix helper emits all four homogeneous coordinates.");
        matrix[0] = 16777216.0f; matrix[1] = 1; matrix[2] = -16777216.0f; matrix[3] = 1;
        require(roomportal::transform(matrix, {1, 1, 1})[0] == 1,
            "Projection preserves separate single-precision operations and original addition order.");
        const roomtraversal::Bounds parent{0, 0, 100, 100};
        const roomportal::Matrix projection{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2};
        auto portal = roomportal::corners({0, 0, -1}, 2, 2);
        require(roomportal::project(portal, projection, parent, parent) == roomtraversal::Bounds{24, 24, 76, 76},
            "Portal projection divides by W before viewport mapping and pixel expansion.");
        portal = roomportal::corners({0, 1, -1}, 2, 2);
        require(roomportal::project(portal, projection, parent, parent) == roomtraversal::Bounds{24, 0, 76, 51},
            "Projection flips Y and clips expanded bounds to the parent.");
        portal = roomportal::corners({0, 0, 1}, 2, 2);
        require(roomportal::project(portal, projection, parent, parent) == roomtraversal::Bounds{},
            "Four positive projected depths reject the portal.");
        portal[0][2] = -1;
        require(roomportal::project(portal, projection, parent, parent) == roomtraversal::Bounds{24, 24, 76, 76},
            "Mixed-depth corners are not replaced with generic near-plane clipping.");
        auto invalidProjection = projection;
        invalidProjection[15] = 0;
        bool undefinedProjection = false;
        try { roomportal::project(portal, invalidProjection, parent, parent); }
        catch (const std::runtime_error&) { undefinedProjection = true; }
        require(undefinedProjection, "Uninitialized original zero-W output is not replaced by invented bounds.");
        require(roomportal::clip(0, parent, {-10, 20, 110, 120}) == roomtraversal::Bounds{0, 20, 100, 100},
            "Portal bounds clamp to their parent.");
        require(roomportal::clip(4, parent, {10, 10, 90, 90}) == roomtraversal::Bounds{} &&
            roomportal::clip(3, parent, {10, 10, 90, 90}) == roomtraversal::Bounds{10, 10, 90, 90},
            "Only four positive-depth corners trigger the original depth rejection.");
        require(roomportal::clip(0, parent, {100, 10, 120, 90}) == roomtraversal::Bounds{} &&
            roomportal::clip(0, parent, {20, 10, 20, 90}) == roomtraversal::Bounds{},
            "Touching and zero-width bounds become the zero sentinel.");
        require(roomportal::clip(0, parent, {70, 20, 30, 80}) == roomtraversal::Bounds{70, 20, 30, 80},
            "Original clipping does not normalize or reject inverted bounds automatically.");
        require(roomportal::enclose({4, 8, -2, 7}, {10, 3, 9, 4}) == roomtraversal::Bounds{-3, 2, 9, 11},
            "Projected bounds expand signed extrema by one pixel.");
        require(roomportal::enclose({-32768, 32767, 0, 0}, {0, 0, 0, 0}) == roomtraversal::Bounds{32767, -1, -32768, 1},
            "Bounds expansion preserves halfword wrap.");
        roomtraversal::State state;
        state.initialize(0x104);
        for (unsigned buffer = 0; buffer < 8; ++buffer) {
            require(state.visitCount(buffer) == 0, "All eight traversal counts start empty.");
            for (unsigned room = 0; room < 4; ++room)
                require(!state.marked(room, buffer, 0), "All allocated room flags start cleared.");
        }
        const roomtraversal::Bounds clipped{-3, 7, 120, 200};
        const roomtraversal::Bounds full{0, 0, 640, 480};
        std::uint32_t roomFlags = 0x80;
        std::uint16_t savedFlags = 0x40;
        state.mark(3, 2, 0x02000170, roomFlags, savedFlags, 4, 9, 1, clipped, full);
        require(roomFlags == 0x82 && savedFlags == 0x41, "Mark preserves existing runtime and saved room bits.");
        require(state.marked(0x102, -1, 3) && !state.marked(2, 0, 3) && state.marked(2, 3, 0),
            "Flag query masks the room index and uses current buffer only for -1.");
        const auto& record = state.record(0x02000170, 3);
        require(record.flags == 3 && record.depth == 4 && record.marker == 9 && record.bounds == clipped &&
            state.visibleMarker(0x02000170, 3) == 9 && state.rawMarker(0x102, 3) == 9,
            "Packed room lookup uses high byte while raw marker uses low-byte index.");
        savedFlags = 0x40;
        state.mark(3, 1, 0x02000170, roomFlags, savedFlags, 5, -1, 2, clipped, full);
        require(savedFlags == 0x40 && record.flags == 3 && record.marker == -1 && record.bounds == full,
            "Non-map2 mark leaves saves alone; sentinel marker does not clear the prior marker flag.");
        require(state.visitCount(3) == 2 && state.visit(3, 0) == 2 && state.visit(3, 1) == 2,
            "Duplicate room visits remain in insertion order.");
        for (unsigned i = 2; i < 64; ++i)
            state.mark(3, 1, 0x02000170, roomFlags, savedFlags, 5, -1, 0, clipped, full);
        std::uint32_t otherFlags = 0;
        state.mark(3, 2, 0x01000003, otherFlags, savedFlags, 8, 2, 1, clipped, full);
        require(state.visitCount(3) == 64 && state.visit(3, 63) == 2 && otherFlags == 2 && savedFlags == 0x41 &&
            state.record(0x01000003, 3).depth == 8 && state.marked(1, 3, 0),
            "A full visit list still updates room, save and traversal record state.");
        state.reset(3, 4);
        require(state.visitCount(3) == 0 && !state.marked(2, 3, 0) && record.marker == -1 && record.depth == 5 &&
            record.bounds == full && state.visibleMarker(0x02000170, 3) == 0,
            "Reset clears flags and visit count but retains the remaining record fields.");
        state.mark(0, 1, 0x03000002, roomFlags, savedFlags, 1, -1, 0, clipped, full);
        require(state.visibleMarker(0x03000002, 0) == -1 && state.record(0x03000002, 0).flags == 1,
            "Marked rooms without marker bit return the original -1 sentinel.");
        state.reset(0, -1);
        require(state.visitCount(0) == 0 && state.marked(3, 0, 0), "Negative signed reset count performs no record iterations.");
        bool rejected = false;
        try { state.marked(4, 0, 0); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "Allocation uses the original low byte of room count.");
        rejected = false;
        try { state.visit(3, 0); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "Cleared visit entries are not exposed by the native API.");
        state.initialize(2);
        require(!state.marked(1, 3, 0) && state.visitCount(0) == 0, "Reinitialization replaces all buffers.");
        std::vector<roomtraversal::SceneRoom> scene{{0x00000100, 0, {0, 1}}, {0x01000101, 0, {0, 1, 2}}, {0x02000102, 0, {2}}};
        doordata::Table links;
        doordata::Descriptor linkDescriptor;
        linkDescriptor.attributes = {1, 0, 0, 0};
        linkDescriptor.identity = 0xff;
        linkDescriptor.center = {0, 0, -1};
        linkDescriptor.dimensions = {2, 2, 0};
        linkDescriptor.rooms = {0, 1};
        links.entries.push_back(linkDescriptor);
        links.entries.push_back(linkDescriptor);
        linkDescriptor.rooms = {1, 2};
        linkDescriptor.identity = 7;
        links.entries.push_back(linkDescriptor);
        std::vector<std::uint32_t> runtimeFlags(3);
        std::vector<std::uint16_t> savedRooms(3);
        state.initialize(3);
        bool blocked = false;
        roomtraversal::Walk traversal(scene, links, state, runtimeFlags, savedRooms, 2,
            {[&](unsigned id) { return blocked && id == 7; }, {}});
        auto submissions = traversal.stationary(0, 0, projection, parent, 1, 0);
        require(submissions.size() == 5 && state.visitCount(0) == 5 && state.visit(0, 1) == 1 &&
            state.visit(0, 2) == 2 && state.visit(0, 3) == 1 && state.visit(0, 4) == 2,
            "Directed portal traversal retains depth-first order and duplicate visits.");
        require(state.record(scene[2].id, 0).depth == 2 && submissions[1].bounds == roomtraversal::Bounds{24, 24, 76, 76} &&
            savedRooms == std::vector<std::uint16_t>({1, 1, 1}), "Portal walk carries clipped bounds, recursion depth and discovery flags.");
        blocked = true;
        submissions = traversal.stationary(0, 0, projection, parent, 1, 0);
        require(submissions.size() == 3 && !state.marked(2, 0, 0) && savedRooms[2] == 1,
            "Closed doors exclude traversal without clearing saved discovery.");
        submissions = traversal.stationary(1, 0, projection, parent, 1, 0);
        require(submissions.size() == 1, "Directed portals do not automatically traverse backward.");
        links.entries[0].attributes[0] = 2;
        submissions = traversal.stationary(1, 0, projection, parent, 1, 0);
        require(submissions.size() == 2 && submissions[1].room == scene[0].id && submissions[1].bounds == parent,
            "Flag-two adjacency accepts the reverse side without portal clipping.");
        links.entries[0].attributes[1] = 3;
        require(traversal.stationary(1, 0, projection, parent, 1, 0).size() == 1,
            "Type-three portals suppress both ordinary traversal branches.");
        std::cout << "Original room traversal storage, mutation and query behavior verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
