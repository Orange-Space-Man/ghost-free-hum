#include "game/events/missionresources.h"
#include "game/audio/gameplayaudiocontext.h"
#include "game/world/mapcollisionstate.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
    unsigned archiveHash(const std::string& name) {
        unsigned hash = 0;
        for (unsigned char character : name) hash = (hash * 3 + character) & 65535;
        return hash;
    }
    void number(std::vector<std::uint8_t>& data, unsigned offset, float value) {
        unsigned bits;
        std::memcpy(&bits, &value, 4);
        word(data, offset, bits);
    }

    std::vector<std::uint8_t> collisionFixture() {
        std::vector<std::uint8_t> data(0x300);
        for (unsigned i = 0; i < 3; ++i) {
            number(data, i * 4, 100);
            number(data, 24 + i * 4, 100);
        }
        word(data, 0x24, 0x40); word(data, 0x28, 0x60); word(data, 0x2c, 0xa0);
        word(data, 0x30, 0x100); word(data, 0x38, 0x180);
        number(data, 0x44, 10);
        number(data, 0x64, 1);
        number(data, 0x6c, -1);
        number(data, 0x80, -1);
        number(data, 0x84, 1); number(data, 0x8c, 1);
        word(data, 0xa8, 0x00010002); word(data, 0xac, 0x00030000);
        number(data, 0xb0, 50); word(data, 0xb4, 0x80000000);
        word(data, 0x100, 0xffff0000);
        word(data, 0x104, 0xffff0000);
        word(data, 0x184, 1);
        return data;
    }
    unsigned fieldHash(const char* name) {
        unsigned hash = 0;
        while (*name) hash = (hash * 256 + static_cast<unsigned char>(*name++)) % 33554393;
        return hash;
    }
    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(0x2c0);
        word(data, 0, 0x52415243); word(data, 4, static_cast<unsigned>(data.size())); word(data, 8, 32);
        word(data, 12, 0x220); word(data, 16, 128);
        word(data, 32, 3); word(data, 36, 0x20);
        word(data, 40, 10); word(data, 44, 0x50); word(data, 48, 0x100); word(data, 52, 0x120);
        word(data, 0x40, 0x524f4f54); word(data, 0x48, 3); word(data, 0x4c, 0);
        word(data, 0x50, 0x4a4d5020); word(data, 0x58, 6); word(data, 0x5c, 3);
        word(data, 0x60, 0x4a4d5020); word(data, 0x68, 1); word(data, 0x6c, 9);
        unsigned stringOffset = 0;
        const auto file = [&](unsigned index, const std::string& name, unsigned flags, unsigned offset, unsigned size) {
            const unsigned entry = 0x70 + index * 20;
            word(data, entry, archiveHash(name)); word(data, entry + 4, (flags << 24) | stringOffset);
            word(data, entry + 8, offset); word(data, entry + 12, size);
            for (char character : name) data[0x140 + stringOffset++] = static_cast<std::uint8_t>(character);
            data[0x140 + stringOffset++] = 0;
        };
        file(0, "jmp", 2, 1, 16); file(1, ".", 2, 0, 16); file(2, "..", 2, 0xffffffff, 16);
        file(3, "characterinfo", 0x11, 0, 16); file(4, "soundpolygoninfo", 0x11, 16, 40);
        file(5, "polygoninfo", 0x11, 56, 16); file(6, "alias", 0x11, 0, 16);
        file(7, ".", 2, 1, 16); file(8, "..", 2, 0, 16); file(9, "late", 0x11, 56, 16);
        word(data, 0x248, 16);
        word(data, 0x254, 2); word(data, 0x258, 40);
        word(data, 0x260, fieldHash("sound_material")); word(data, 0x264, 0xffffffff);
        word(data, 0x26c, fieldHash("sound_echo_switch")); word(data, 0x270, 0xffffffff);
        word(data, 0x280, 16);
        return data;
    }
}

int main(int argc, char** argv) {
    try {
        const auto data = fixture();
        mission::Resources resources;
        std::string error;
        require(resources.read(data, error), error.c_str());
        require(resources.tableCount() == 4, "Mission table count uses declared directory count minus two.");
        require(resources.archive().directory("/JMP").size() == 6, "Archive directory enumeration preserves dot entries and folds requested case.");
        require(resources.archive().findNameResource("LATE") == 9 &&
            resources.archive().findPathResource("/JMP/CHARACTERINFO") == 3 &&
            resources.archive().findPathResource("/jmp") == -1 &&
            resources.archive().findPathResource("/jmp/characterinfo/extra") == -1 &&
            resources.archive().findPathResource("jmp/characterinfo") == -1,
            "Global names span nodes while absolute paths enforce file and directory boundaries.");
        const auto* character = resources.getJmpResource("CharacterInfo");
        require(character && character == resources.getJmpResource("CHARACTERINFO"), "Repeated lookups return the same owned ToolData.");
        require(resources.getJmpResource("alias") == character, "Payload aliases resolve to the first attached table, matching resource pointer identity.");
        require(!resources.getJmpResource("late"), "Typed lookup stops at the first matching node even when a later node contains the name.");
        require(!resources.getJmpResource("absent") && resources.archive().findTypeResource(0, "characterinfo") == -1, "Missing type/name lookup returns no resource.");
        auto damaged = data;
        auto pathArchive = data;
        word(pathArchive, 0x50, 0x50415448);
        mission::Resources paths;
        require(paths.read(pathArchive, error), error.c_str());
        const auto* firstPath = paths.getPathResource("CharacterInfo");
        require(firstPath && firstPath->attached() && firstPath == paths.getPathResource("ALIAS"),
            "PATH resources preserve shared payload identity and case folding.");
        require(paths.getPathResource("polygoninfo") != firstPath && !paths.getPathResource("late") &&
            !paths.getPathResource("absent"), "PATH lookup honors typed node boundaries.");
        auto damagedPaths = pathArchive;
        word(damagedPaths, 0x70 + 3 * 20 + 12, 0xffffff);
        require(!paths.read(damagedPaths, error) && paths.getPathResource("alias") == firstPath,
            "Failed mission replacement preserves cached PATH ownership.");
        require(paths.read(data, error) && !paths.getPathResource("alias"),
            "Successful mission replacement discards the old PATH cache.");
        word(damaged, 0x70 + 3 * 20 + 12, 0xffffff);
        require(!resources.read(damaged, error) && resources.getJmpResource("CharacterInfo") == character,
            "Failed resource replacement preserves the prior owner and table pointers.");
        mapcollision::State collision(resources);
        require(collision.mask() == 0x8000 && collision.soundMaterialField() == -1 && collision.soundEchoField() == -1,
            "Original MapCol constructor state.");
        collision.load({1, 2, 3, 4});
        require(collision.soundMaterialField() == 0 && collision.soundEchoField() == 1 &&
            collision.polygonInfo() == resources.getJmpResource("PolygonInfo"), "Collision state binds original mission tables.");
        collision.enable(0); collision.enable(16); collision.enable(32); collision.disable(15);
        require(collision.mask() == 1, "Original ushort mask truncation and PPC shifts.");
        collision.disable(64);
        require(collision.mask() == 0, "PPC mask shift uses the low six shift bits.");
        collision.enable(15);
        auto floor = collisionFixture();
        word(floor, 0x180, 1);
        collision.load(floor);
        auto contact = collision.sphereContact({10, 12, 10}, 5);
        require(contact.floor == 0 && contact.wall == -1 && contact.correction == std::array<float, 3>{0, 3, 0} &&
            contact.floorNormal == std::array<float, 3>{0, 1, 0}, "Sphere contact retains the floor polygon and penetration correction.");
        require(collision.sphereContact({10, 9, 10}, 5).floor == -1 &&
            collision.sphereContact({10, 16, 10}, 5).floor == -1,
            "Original sphere test rejects the back face and spheres above their radius.");
        require(collision.sphereContact({10, 15, 10}, 5).floor == 0,
            "Touching the plane at exactly the radius still reports contact.");
        require(collision.floorPolygons({10, 15, 10}, 5) == std::array<std::uint16_t, 4>{0, 0xffff, 0xffff, 0xffff} &&
            collision.floorPolygons({10, 16, 10}, 5)[0] == 0xffff,
            "Original floor probes include distance equality and fill unused polygon slots with the sentinel.");
        contact = collision.sweepSphere({10, 8, 10}, 5, {0, -12, 0});
        require(contact.floor == 0 && contact.correction[1] == 7,
            "Original substeps stop a descending sphere before its final center crosses behind the floor.");
        require(collision.sweepSphere({10, 8, 10}, 5, {0, -1000, 0}).floor == -1,
            "Original displacement cutoff disables substeps at one thousand units.");
        collision.disable(15);
        require(collision.sphereContact({10, 12, 10}, 5).floor == -1, "Sphere tests honor the current collision mask.");
        collision.enable(15);
        std::array<float, 3> position{10, 20, 10};
        require(collision.projectFloor(position) && position == std::array<float, 3>{10, 10, 10},
            "Projection uses the selected cell, plane and three original polygon bounds.");
        for (const auto point : {std::array<float, 3>{30, 20, 30}, {-1, 20, 10}, {100, 20, 0}, {10, 9, 10}}) {
            auto unchanged = point;
            require(!collision.projectFloor(unchanged) && unchanged == point,
                "Projection retains position outside the polygon, outside the grid, or below the plane.");
        }
        position = {25, 10, 25};
        require(collision.projectFloor(position), "Original projection includes plane and edge equality.");
        collision.disable(15);
        require(collision.floorHeight(position) == -32000, "Disabled polygon mask suppresses projection.");
        collision.enable(15);
        number(floor, 0x44, -30000);
        collision.load(floor);
        position = {10, 20, 10};
        require(collision.projectFloor(position) && position[1] == -30000, "Projection includes its lower success threshold.");
        number(floor, 0x44, -30001);
        collision.load(floor);
        position = {10, 20, 10};
        require(!collision.projectFloor(position) && collision.floorHeight(position) == -30001,
            "A valid lower plane remains a query result but does not move the effect.");
        auto edges = collisionFixture();
        word(edges, 0x180, 1);
        word(edges, 0x24, 0xc0);
        word(edges, 0xa0, 0x00000001);
        word(edges, 0xa4, 0x00020000);
        for (unsigned i = 0; i < 9; ++i) {
            const float vertices[]{10, 10, 10, 10, 10, 60, 60, 10, 10};
            number(edges, 0xc0 + i * 4, vertices[i]);
        }
        collision.load(edges);
        require(collision.sphereContact({9, 12, 20}, 5).floor == -1,
            "Disabled triangle edges reject spheres whose projected center lies outside the face.");
        word(edges, 0xb4, 0x80000001);
        collision.load(edges);
        contact = collision.sphereContact({9, 12, 20}, 5);
        require(contact.floor == 0 && contact.correction[0] < 0 && contact.correction[1] > 0,
            "Enabled original edge flags resolve sphere penetration away from the closest edge point.");
        require(collision.sphereContact({4, 12, 20}, 5).floor == -1,
            "Sphere-edge rejection honors the original radius bound.");
        number(edges, 0x60, 1); number(edges, 0x64, 0);
        number(edges, 0x6c, 0); number(edges, 0x70, -1);
        number(edges, 0x84, 0); number(edges, 0x88, 1);
        collision.load(edges);
        contact = collision.sphereContact({12, 20, 20}, 5);
        require(contact.wall == 0 && contact.floor == -1 && contact.correction == std::array<float, 3>{3, 0, 0},
            "Wall contact uses the original normal classification and retains its independent polygon identifier.");
        auto noSound = data;
        word(noSound, 0x70 + 4 * 20, 0);
        require(resources.read(noSound, error), error.c_str());
        collision.load({});
        require(collision.soundMaterialField() == 0 && collision.soundEchoField() == 1,
            "Missing sound table on reload preserves cached fields as in the original.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, collisionBytes;
            require(package.open(argv[1], error) && package.read("files/Map/map2.szp", compressed, error) &&
                roomarchive::decompress(compressed, archive, error), error.c_str());
            require(resources.read(archive, error), error.c_str());
            require(resources.tableCount() == 25, "Owned map2 mission table count.");
            gameflags::State audioFlags;
            gameplayaudio::Context audioContext(resources, audioFlags, 2);
            const auto* roomInfo = resources.getJmpResource("RoomInfo");
            require(roomInfo && roomInfo->count() > 0x29, "Original room audio exception row exists.");
            std::uint32_t specialRoom = 0;
            require(roomInfo->getValue(0x29, "RoomNo", specialRoom), "Original exception room identity exists.");
            const auto ordinaryAudio = audioContext.parameters(specialRoom, -1);
            audioFlags.setOn(0x31);
            require(audioContext.parameters(specialRoom, -1).packed == 0x280, "First mission flag must replace room audio parameters.");
            audioFlags.setOff(0x31);
            audioFlags.setOn(0x32);
            require(audioContext.parameters(specialRoom, -1).packed == 0x280, "Second mission flag must replace room audio parameters.");
            audioFlags.setOff(0x32);
            require(audioContext.parameters(specialRoom, -1).packed == ordinaryAudio.packed, "Room audio must observe live flags rather than cached flags.");
            require(audioContext.parameters(specialRoom | 0x12000000, -1).packed == ordinaryAudio.packed,
                "Room lookup uses the low byte of the original room identity.");
            const auto* soundInfo = resources.getJmpResource("SoundPolygonInfo");
            require(soundInfo && soundInfo->count(), "Original floor audio table exists.");
            for (unsigned row = 0; row < soundInfo->count(); ++row) {
                std::uint32_t material = 0, echo = 0;
                require(soundInfo->getValue(row, "sound_material", material) && soundInfo->getValue(row, "sound_echo_switch", echo),
                    "Original floor audio fields exist.");
                const auto packed = audioContext.parameters(0xffffffff, static_cast<std::int16_t>(row));
                require(packed.packed == (((material & 15) | ((echo & 7) << 4)) << 16),
                    "Floor material and echo use their original packed widths.");
            }
            const auto* characters = resources.getJmpResource("CharacterInfo");
            require(characters && characters->findEntryByValue(characters->searchItemInfo("name"), "luige") >= 0,
                "Typed original CharacterInfo lookup feeds shared ToolData.");
            require(roomarchive::read(archive, "col.mp", collisionBytes, error), error.c_str());
            collision.load(collisionBytes);
            require(collision.soundMaterialField() >= 0 && collision.soundEchoField() >= 0 && collision.polygonInfo(),
                "Owned collision initialization resolves sound and polygon tables.");
            unsigned projected = 0;
            unsigned contacted = 0;
            for (unsigned row = 0; row < characters->count(); ++row) {
                std::array<float, 3> position{};
                if (!characters->getValue(row, "pos_x", position[0]) || !characters->getValue(row, "pos_y", position[1]) ||
                    !characters->getValue(row, "pos_z", position[2])) continue;
                if (collision.projectFloor(position)) {
                    ++projected;
                    position[1] += 1;
                    const auto contact = collision.sphereContact(position, 5);
                    if (contact.floor != -1) {
                        ++contacted;
                        std::array<float, 3> normal;
                        require(collision.polygonNormal(contact.floor, normal) && normal == contact.floorNormal,
                            "Imported contact retains the exact collision polygon normal.");
                    }
                }
            }
            require(projected > 0, "Original map2 cell lists and polygon records project placed characters.");
            require(contacted > 0, "Original map2 sphere lists resolve placed-character floor contacts.");
            std::cout << "25 mission tables loaded with stable typed lookup; original collision table bindings verified.\n";
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
