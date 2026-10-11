#include "game/world/roomgeometry.h"

#include <iostream>

namespace {
    void write16(std::vector<std::uint8_t>& data, std::size_t offset, std::uint16_t value) {
        data[offset] = static_cast<std::uint8_t>(value >> 8);
        data[offset + 1] = static_cast<std::uint8_t>(value);
    }

    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        write16(data, offset, static_cast<std::uint16_t>(value >> 16));
        write16(data, offset + 2, static_cast<std::uint16_t>(value));
    }

    std::vector<std::uint8_t> makeModel(std::uint8_t primitive, std::uint16_t count) {
        std::vector<std::uint8_t> data(0x150, 0);
        data[0] = 2;
        write32(data, 0x14, 0x60);
        write32(data, 0x38, 0x80);
        write32(data, 0x3c, 0xc0);
        write16(data, 0x66, 10);
        write16(data, 0x6e, 10);
        write16(data, 0x72, 10);
        write16(data, 0x74, 10);
        write16(data, 0x82, 1);
        write32(data, 0x84, 0x200);
        write32(data, 0x8c, 0x20);
        data[0xa0] = primitive;
        write16(data, 0xa1, count);
        for (std::uint16_t index = 0; index < count; ++index) {
            write16(data, 0xa3 + index * 2, index);
        }
        write16(data, 0xc2, 0xffff);
        write16(data, 0xc4, 0xffff);
        write32(data, 0xcc, 0x3f800000);
        write32(data, 0xd0, 0x3f800000);
        write32(data, 0xd4, 0x3f800000);
        write32(data, 0xe4, 0x40a00000);
        write16(data, 0x10c, 1);
        write32(data, 0x110, 0x8c);
        return data;
    }
}

int main() {
    int failures = 0;
    const auto check = [&](bool success, const char* description) {
        if (!success) {
            std::cerr << description << '\n';
            ++failures;
        }
    };
    roomgeometry::Mesh mesh;
    std::string error;
    auto data = makeModel(0x90, 3);
    const auto original = data;
    check(roomgeometry::decode(data, mesh, error), "Triangle decode");
    check(mesh.triangles.size() == 1 && mesh.minimum[0] == 5 && mesh.maximum[0] == 15 && mesh.maximum[1] == 10, "Positions and translation");
    check(data == original, "Read-only geometry input");
    roommodel::Node pose;
    pose.scale = {1, 1, 1};
    pose.translation = {20, 0, 0};
    check(roomgeometry::decode(data, mesh, error, {pose}) && mesh.minimum[0] == 20 && mesh.maximum[0] == 30,
        "Pose replaces model transform without changing hierarchy");
    for (const std::uint8_t command : {0x80, 0x98, 0xa0}) {
        data = makeModel(command, 4);
        check(roomgeometry::decode(data, mesh, error) && mesh.triangles.size() == 2, "Quad, strip or fan decode");
    }
    data = original;
    data.resize(0x168);
    write32(data, 0x24, 0x150);
    write32(data, 0x84, 0x2200);
    for (unsigned index = 0; index < 3; ++index) {
        write16(data, 0xa3 + index * 4, static_cast<std::uint16_t>(index));
        write16(data, 0xa5 + index * 4, static_cast<std::uint16_t>(index));
    }
    write32(data, 0x158, 0x3f800000);
    write32(data, 0x164, 0x3f800000);
    check(roomgeometry::decode(data, mesh, error) && mesh.triangles[0].coordinates[1][0] == 1 &&
        mesh.triangles[0].coordinates[2][1] == 1, "Indexed float texture coordinates");
    write16(data, 0xa5, 0xffff);
    check(!roomgeometry::decode(data, mesh, error), "Reject invalid texture coordinate index");
    data = original;
    write16(data, 0xa3, 0xffff);
    check(!roomgeometry::decode(data, mesh, error) && mesh.triangles.empty(), "Reject invalid position index");
    data = original;
    write16(data, 0xa1, 0xffff);
    check(!roomgeometry::decode(data, mesh, error), "Reject truncated primitive");
    data = original;
    data[0xa0] = 0x61;
    check(!roomgeometry::decode(data, mesh, error), "Reject unsupported display command");
    data = original;
    write32(data, 0x84, 1);
    check(!roomgeometry::decode(data, mesh, error), "Reject missing positions");
    data = original;
    write16(data, 0x14e, 0xffff);
    check(!roomgeometry::decode(data, mesh, error), "Reject invalid batch index");
    data = original;
    write32(data, 0x8c, 0xfffffff0);
    check(!roomgeometry::decode(data, mesh, error), "Reject invalid display-list offset");
    data = original;
    write32(data, 0x110, 0xfffffff0);
    check(!roomgeometry::decode(data, mesh, error), "Reject invalid part table");
    data = original;
    write16(data, 0xc2, 0);
    check(!roomgeometry::decode(data, mesh, error), "Reject hierarchy cycle");
    return failures == 0 ? 0 : 1;
}
