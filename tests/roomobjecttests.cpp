#include "resources/roomarchivereader.h"
#include "game/world/roomobjectplacement.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace {
    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        for (unsigned index = 0; index < 4; ++index) data[offset + index] = static_cast<std::uint8_t>(value >> (24 - index * 8));
    }

    void writeFloat(std::vector<std::uint8_t>& data, std::size_t offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        write32(data, offset, bits);
    }

    std::vector<std::uint8_t> furniture() {
        const char* names[]{"room_no", "dmd_name", "pos_x", "pos_y", "pos_z", "dir_x", "dir_y", "dir_z", "scale_x", "scale_y", "scale_z"};
        std::vector<std::uint8_t> data(148 + 72 * 2);
        write32(data, 0, 2);
        write32(data, 4, 11);
        write32(data, 8, 148);
        write32(data, 12, 72);
        for (unsigned index = 0; index < 11; ++index) {
            std::uint32_t hash = 0;
            for (const char* name = names[index]; *name; ++name) hash = ((hash << 8) + *name) % 33554393;
            const unsigned type = index == 0 ? 0 : index == 1 ? 1 : 2;
            const unsigned offset = index == 0 ? 0 : index == 1 ? 4 : 36 + (index - 2) * 4;
            write32(data, 16 + index * 12, hash);
            write32(data, 20 + index * 12, index == 0 ? 0xff00 : 0);
            write32(data, 24 + index * 12, (offset << 16) | (index == 0 ? 8 << 8 : 0) | type);
        }
        for (unsigned record = 0; record < 2; ++record) {
            const auto offset = 148 + record * 72;
            write32(data, offset, record << 8);
            std::memcpy(data.data() + offset + 4, "chair", 6);
            for (unsigned axis = 0; axis < 3; ++axis) writeFloat(data, offset + 60 + axis * 4, 1);
        }
        writeFloat(data, 148 + 36, 10);
        writeFloat(data, 148 + 52, 90);
        return data;
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
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> compressed(23);
    write32(compressed, 0, 0x59617930);
    write32(compressed, 4, 6);
    write32(compressed, 8, 20);
    write32(compressed, 12, 22);
    write32(compressed, 16, 0x80000000);
    compressed[20] = 0x30;
    compressed[22] = 'A';
    check(roomarchive::decompress(compressed, output, error) && output == std::vector<std::uint8_t>(6, 'A'), "Yay0 overlapping back reference");
    compressed[21] = 1;
    check(!roomarchive::decompress(compressed, output, error) && output.empty(), "Reject Yay0 reference before output");
    compressed[21] = 0;
    write32(compressed, 4, 5);
    check(!roomarchive::decompress(compressed, output, error), "Reject Yay0 output overrun");
    compressed[20] = 0;
    compressed.push_back(0);
    write32(compressed, 4, 19);
    check(roomarchive::decompress(compressed, output, error) && output.size() == 19, "Yay0 extended run length");
    compressed.resize(23);
    check(!roomarchive::decompress(compressed, output, error), "Reject truncated Yay0 extended length");
    write32(compressed, 4, 0xffffffff);
    check(!roomarchive::decompress(compressed, output, error), "Reject oversized Yay0 output");

    std::vector<std::uint8_t> archive(0xd3);
    write32(archive, 0, 0x52415243);
    write32(archive, 4, static_cast<std::uint32_t>(archive.size()));
    write32(archive, 8, 0x20);
    write32(archive, 12, 0xb0);
    write32(archive, 16, 3);
    write32(archive, 32, 2);
    write32(archive, 36, 0x20);
    write32(archive, 40, 2);
    write32(archive, 44, 0x40);
    write32(archive, 48, 16);
    write32(archive, 52, 0x80);
    write32(archive, 0x48, 1);
    write32(archive, 0x58, 1);
    write32(archive, 0x5c, 1);
    write32(archive, 0x64, 0x02000000);
    write32(archive, 0x68, 1);
    write32(archive, 0x78, 0x11000004);
    write32(archive, 0x80, 3);
    std::memcpy(archive.data() + 0xa0, "jmp\0furniture\0", 14);
    archive[0xd0] = 1;
    archive[0xd1] = 2;
    archive[0xd2] = 3;
    check(roomarchive::read(archive, "jmp/furniture", output, error) && output == std::vector<std::uint8_t>({1, 2, 3}), "Nested RARC lookup");
    check(!roomarchive::read(archive, "jmp/missing", output, error) && output.empty(), "Reject missing RARC file");
    bool missing = false;
    check(!roomarchive::read(archive, "jmp/missing", output, error, &missing) && missing, "Identify absent optional model");
    write32(archive, 0x68, 2);
    check(!roomarchive::read(archive, "jmp/furniture", output, error, &missing) && !missing, "Malformed archive is not an absent model");
    write32(archive, 0x68, 1);
    write32(archive, 0x80, 4);
    check(!roomarchive::read(archive, "jmp/furniture", output, error), "Reject RARC payload overrun");

    auto data = furniture();
    std::vector<roomobjects::Placement> placements;
    check(roomobjects::read(data, 0, placements, error) && placements.size() == 1 && placements[0].model == "chair", "JMP fields and masked room selection");
    if (!placements.empty()) {
        placements[0].scale = {2, 1, 1};
        const auto point = roomobjects::transform({1, 0, 0}, placements[0]);
        check(std::abs(point[0] - 10) < 0.0001f && std::abs(point[2] + 2) < 0.0001f, "Scale then rotate then translate");
    }
    writeFloat(data, 148 + 36, std::nanf(""));
    check(!roomobjects::read(data, 0, placements, error) && placements.empty(), "Reject non-finite placement");
    data = furniture();
    write32(data, 24, 0x00470000);
    check(!roomobjects::read(data, 0, placements, error), "Reject field outside JMP record");
    data = furniture();
    data.pop_back();
    check(!roomobjects::read(data, 0, placements, error), "Reject truncated JMP records");
    return failures == 0 ? 0 : 1;
}
