#include "resources/glme01addresses.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "game/events/missionresources.h"
#include "game/enemies/enemystrategycatalog.h"
#include "game/enemies/enemytypecatalog.h"
#include "game/world/roomlocation.h"
#include "game/world/roomdependencies.h"
#include "game/scene/gameplayspawn.h"
#include "game/events/missioneventactivation.h"
#include "resources/dolresources.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected the imported asset package path.");
        gameassets::Package package;
        std::string error;
        std::vector<std::uint8_t> executable, compressed, archive;
        if (!package.open(argv[1], error) || !package.read("sys/main.dol", executable, error) ||
            !package.read("files/Map/map2.szp", compressed, error) || !roomarchive::decompress(compressed, archive, error))
            throw std::runtime_error(error);
        mission::Resources mission;
        enemystrategy::Catalog strategies;
        enemytypes::Catalog types;
        if (!mission.read(archive, error) || !strategies.read(executable, error) || !types.read(executable, error))
            throw std::runtime_error(error);
        std::vector<std::uint8_t> characters;
        gameplayspawn::Spawn spawn;
        if (!roomarchive::read(archive, "jmp/characterinfo", characters, error) ||
            !gameplayspawn::readCharacter(characters, 0, spawn, error) || !gameplayspawn::resolveRoom(executable, 2, spawn, error))
            throw std::runtime_error(error);
        roomlocation::Rooms rooms(executable, 2);
        roomdependencies::Catalog dependencies(executable, 2);
        std::vector<std::uint8_t> initialFlags;
        gamesave::State save;
        gameentry::State entry;
        if (!dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error) ||
            !gamesave::initialize(initialFlags, save, error) || !gameentry::selectNewFile(save, 0, entry, error)) {
            throw std::runtime_error(error);
        }
        gameflags::State flags;
        flags.restore(entry.flags);
        gameevent::MissionActivation events(mission, types, entry, flags);
        std::cout << "Spawn room: " << std::hex << spawn.roomId << std::dec << '\n';
        std::cout << "Original dependent rooms:";
        for (auto room : *dependencies.list(spawn.roomId)) std::cout << ' ' << room;
        std::cout << '\n';
        for (const char* tableName : {"ObjInfo", "CharacterInfo", "KeyInfo", "GeneratorInfo", "EnemyInfo", "ObserverInfo"}) {
            const auto* table = mission.getJmpResource(tableName);
            if (!table) continue;
            for (unsigned row = 0; row < table->count(); ++row) {
                std::array<float, 3> position{};
                table->getValue(row, "pos_x", position[0]);
                table->getValue(row, "pos_y", position[1]);
                table->getValue(row, "pos_z", position[2]);
                const auto room = rooms.resolve(position);
                std::uint32_t stay = 0, appear = 0, disappear = 0, event = 0;
                table->getValue(row, "stay", stay);
                table->getValue(row, "appear_flag", appear);
                table->getValue(row, "disappear_flag", disappear);
                table->getValue(row, "event_set_no", event);
                if (!dependencies.contains(spawn.roomId, room & 255) && !stay) continue;
                const bool admitted = (event && events.activation().value(static_cast<std::int16_t>(event))) ||
                    ((!appear || flags.value(static_cast<std::uint8_t>(appear))) &&
                        (!disappear || !flags.value(static_cast<std::uint8_t>(disappear))));
                std::string_view name, createName, generated;
                table->getValue(row, "name", name);
                table->getValue(row, "create_name", createName);
                const bool generator = name == "generator";
                if (generator) table->getValue(row, "type", generated);
                const auto type = types.find(generator ? generated : name);
                if (type >= 381) continue;
                const auto selected = generator ? strategies.selectGenerator(type) : strategies.select(type, {table, static_cast<int>(row)});
                std::cout << tableName << " row " << row << " " << name << (generator ? ":" : "") << generated << " create=" << createName
                    << " room=" << (room & 255) << " stay=" << stay << " appear=" << appear << " disappear=" << disappear << " event=" << event
                    << " fresh=" << admitted << " strategy=" << selected << " factory=" << std::hex << strategies.constructor(selected) << std::dec << '\n';
            }
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
