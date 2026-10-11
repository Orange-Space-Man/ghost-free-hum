#include "game/world/roomdoorplacement.h"
#include "game/world/roommodelpose.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace {
    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        for (unsigned index = 0; index < 4; ++index) data[offset + index] = static_cast<std::uint8_t>(value >> (24 - index * 8));
    }

    void write16(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value) {
        data[offset] = static_cast<std::uint8_t>(value >> 8);
        data[offset + 1] = static_cast<std::uint8_t>(value);
    }

    void writeFloat(std::vector<std::uint8_t>& data, std::size_t offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        write32(data, offset, bits);
    }
}

int main() {
    unsigned failures = 0;
    const auto check = [&](bool condition, const char* description) {
        if (!condition) {
            std::cerr << description << '\n';
            ++failures;
        }
    };
    std::string error;
    std::vector<std::uint8_t> animation(0x80);
    animation[0] = 1;
    write32(animation, 4, 10);
    write32(animation, 8, 0x50);
    write32(animation, 12, 0x54);
    write32(animation, 16, 0x70);
    write32(animation, 20, 24);
    writeFloat(animation, 0x50, 1);
    writeFloat(animation, 0x58, 90);
    writeFloat(animation, 0x60, 9);
    writeFloat(animation, 0x64, 120);
    writeFloat(animation, 0x70, 100);
    for (unsigned channel = 0; channel < 9; ++channel) write16(animation, 24 + channel * 6, channel >= 3 && channel < 6 ? 2 : 1);
    std::vector<roommodel::Node> pose;
    check(roompose::readInitial(animation, 1, pose, error) && pose.size() == 1 && pose[0].scale[0] == 1 &&
        pose[0].rotation[1] == 90 && pose[0].translation[0] == 100, "Initial ANM constant and animated channels");
    writeFloat(animation, 0x54, 2);
    check(!roompose::readInitial(animation, 1, pose, error) && pose.empty(), "Reject animation starting after frame zero");
    writeFloat(animation, 0x54, 0);
    write16(animation, 26, 0xffff);
    check(!roompose::readInitial(animation, 1, pose, error), "Reject ANM channel index outside key table");
    write16(animation, 26, 0);
    writeFloat(animation, 0x70, std::nanf(""));
    check(!roompose::readInitial(animation, 1, pose, error), "Reject non-finite ANM pose");
    check(!roompose::readInitial(animation, 2, pose, error), "Reject missing ANM node");

    std::vector<std::uint8_t> dol(0x1100);
    write32(dol, 0, 0x100);
    write32(dol, 0x48, 0x802ff000);
    write32(dol, 0x90, 0x1000);
    write32(dol, 0x320, 0x802ffb00);
    write32(dol, 0xc18, 0x802ffc00);
    dol[0xd00] = 1;
    write32(dol, 0xd04, 0x100);
    write32(dol, 0xd08, 300);
    write32(dol, 0xd0c, 150);
    write32(dol, 0xd10, 500);
    write16(dol, 0xd16, 300);
    dol[0xd1a] = 0;
    dol[0xd1b] = 5;
    dol[0xa70] = 1;
    write32(dol, 0xa74, 100);
    write32(dol, 0x96c, 0x802ffd00);
    std::memcpy(dol.data() + 0xe00, "/iwamoto/Door/door_01.bin", 26);
    std::vector<roomobjects::Placement> doors;
    check(roomdoors::read(dol, 0, doors, error) && doors.size() == 1 && doors[0].model == "iwamoto/door/door_01.bin" &&
        doors[0].position == roomgeometry::Position{400, 0, 500}, "DOL address translation and door placement");
    check(roomdoors::read(dol, 7, doors, error) && doors.empty(), "Door room filtering");
    dol[0xd00] = 2;
    dol[0xa70] = 2;
    check(roomdoors::read(dol, 0, doors, error) && doors.size() == 2 &&
        doors[0].position[2] == 600 && doors[1].position[2] == 400 &&
        doors[0].rotation[1] == -90 && doors[1].rotation[1] == 90, "Sideways double-door leaves");
    write32(dol, 0xc18, 0x70000000);
    check(!roomdoors::read(dol, 0, doors, error) && doors.empty(), "Reject unmapped door data pointer");
    check(!roomdoors::read({}, 0, doors, error), "Reject truncated DOL header");
    return failures == 0 ? 0 : 1;
}
