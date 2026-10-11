#include "game/scene/gameplayspawn.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
    unsigned hash(const char* name) {
        unsigned result = 0;
        while (*name) result = ((result << 8) + static_cast<unsigned char>(*name++)) % 33554393;
        return result;
    }
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main(int argc, char** argv) {
    try {
        std::vector<std::uint8_t> data(76 + 3 * 48);
        word(data, 0, 3); word(data, 4, 5); word(data, 8, 76); word(data, 12, 48);
        const char* names[]{"name", "appear_point", "pos_x", "pos_y", "pos_z"};
        for (unsigned i = 0; i < 5; ++i) {
            word(data, 16 + i * 12, hash(names[i]));
            word(data, 20 + i * 12, 0xffffffff);
            word(data, 24 + i * 12, ((i == 0 ? 0 : 28 + i * 4) << 16) | (i == 0 ? 1 : i == 1 ? 0 : 2));
        }
        for (unsigned i = 0; i < 3; ++i) {
            std::memcpy(data.data() + 76 + i * 48, i == 0 ? "ghost" : "luige", 6);
            word(data, 76 + i * 48 + 32, i == 1 ? 1 : 0);
            word(data, 76 + i * 48 + 36, 0x42c80000);
        }
        gameplayspawn::Spawn spawn;
        std::string error;
        require(gameplayspawn::readCharacter(data, 0, spawn, error), error.c_str());
        require(spawn.characterRecord == 2 && spawn.position[0] == 100, "Match Luigi name and appear point, preserving table order.");
        auto noAppearPoint = data;
        word(noAppearPoint, 28, hash("unused_field"));
        require(gameplayspawn::readCharacter(noAppearPoint, 0, spawn, error), error.c_str());
        require(spawn.characterRecord == 1, "Original missing appear-point field defaults to zero.");
        require(gameplayspawn::readCharacter(data, 9, spawn, error), error.c_str());
        require(spawn.characterRecord == -1 && spawn.position == std::array<float, 3>{}, "Original missing-entry origin fallback.");
        std::vector<std::uint8_t> oriented(112 + 60);
        std::copy(data.begin(), data.begin() + 76, oriented.begin());
        word(oriented, 0, 1); word(oriented, 4, 8); word(oriented, 8, 112); word(oriented, 12, 60);
        std::copy(data.begin() + 76 + 2 * 48, data.begin() + 76 + 3 * 48, oriented.begin() + 112);
        const char* directions[]{"dir_x", "dir_y", "dir_z"};
        for (unsigned axis = 0; axis < 3; ++axis) {
            word(oriented, 76 + axis * 12, hash(directions[axis]));
            word(oriented, 80 + axis * 12, 0xffffffff);
            word(oriented, 84 + axis * 12, ((48 + axis * 4) << 16) | 2);
        }
        word(oriented, 160, 0xc2b40000);
        word(oriented, 164, 0x43340000);
        word(oriented, 168, 0x43b40000);
        require(gameplayspawn::readCharacter(oriented, 0, spawn, error), error.c_str());
        require(spawn.rotation == std::array<std::int16_t, 3>{-16384, -32768, 0}, "Original negative, half-turn and full-turn angle conversion.");
        word(oriented, 164, 0x7fc00000);
        require(!gameplayspawn::readCharacter(oriented, 0, spawn, error), "Reject nonfinite character direction.");
        data.pop_back();
        require(!gameplayspawn::readCharacter(data, 0, spawn, error), "Reject truncated character records.");
        std::vector<std::uint8_t> executable(0x500);
        const auto section = [&](unsigned index, unsigned file, unsigned address, unsigned size) {
            word(executable, index * 4, file);
            word(executable, 0x48 + index * 4, address);
            word(executable, 0x90 + index * 4, size);
        };
        section(0, 0x100, 0x802ff220, 4);
        section(1, 0x120, 0x80300000, 0x300);
        section(2, 0x420, 0x804d9ab8, 4);
        word(executable, 0x100, 0x80300000);
        executable[0x120] = 1;
        word(executable, 0x124, 0x80300100);
        word(executable, 0x134, 0x80300040);
        word(executable, 0x420, 0x42200000);
        word(executable, 0x160, 0x100);
        for (unsigned axis = 0; axis < 3; ++axis) word(executable, 0x160 + 20 + axis * 4, 100);
        word(executable, 0x220, 0x80300120);
        std::memcpy(executable.data() + 0x240, "/Iwamoto/map2/room_00.arc", 26);
        spawn = {};
        require(gameplayspawn::resolveRoom(executable, 2, spawn, error), "Room bounds include their lower edges.");
        require(spawn.roomId == 0x100 && spawn.position[1] == 0, "Room query adds original height without moving spawn.");
        spawn.position[0] = 100;
        require(!gameplayspawn::resolveRoom(executable, 2, spawn, error), "Room bounds exclude upper edges.");
        spawn.position = {0, -40, 0};
        require(gameplayspawn::resolveRoom(executable, 2, spawn, error), "Original 40-unit room query height.");
        executable[0x180] = 1;
        word(executable, 0x184, 0x80300180);
        for (unsigned axis = 0; axis < 3; ++axis) {
            word(executable, 0x2a0 + axis * 4, 10);
            word(executable, 0x2ac + axis * 4, 20);
        }
        require(!gameplayspawn::resolveRoom(executable, 2, spawn, error), "Subregions replace the outer room bounds.");
        spawn.position = {10, -30, 10};
        require(gameplayspawn::resolveRoom(executable, 2, spawn, error), "Subregion lower boundary match.");
        if (argc == 2) {
            roomscene::Scene room;
            if (!gameplayspawn::load(argv[1], spawn, room, error)) throw std::runtime_error(error);
            require(!room.mesh.triangles.empty(), "Resolved room contains geometry.");
            std::cout << "Spawn record " << spawn.characterRecord << ": " << spawn.position[0] << ", " << spawn.position[1] << ", " << spawn.position[2]
                << "; room ID " << std::hex << spawn.roomId << std::dec << "; " << spawn.roomArchive << "; triangles " << room.mesh.triangles.size() << '\n';
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
