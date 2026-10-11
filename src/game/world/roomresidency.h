#pragma once

#include "resources/gameassetpackage.h"
#include "game/scene/gamearchive.h"
#include "game/world/doordescriptors.h"
#include "game/world/roomlocation.h"
#include "game/world/doorresources.h"
#include "game/world/roomfurnitureresources.h"

namespace roomresidency {
    struct Resident {
        unsigned room = 0xffffffff;
        std::string path;
        gamearchive::Archive archive;
        std::vector<std::uint8_t> model;
        doors::ModelResource drawing;
        roomfurniture::Resources furniture;
    };

    class InitialRooms {
    public:
        InitialRooms(const std::vector<std::uint8_t>& executable, unsigned map, const roomlocation::Rooms& rooms);
        std::vector<unsigned> required(const std::vector<std::uint32_t>& playerRooms) const;
        void load(gameassets::Package& package, const std::vector<std::uint32_t>& playerRooms,
            const jmp::ToolData& furniture, const gameflags::State& flags);
        const std::array<Resident, 7>& slots() const;
        const doordata::Table& descriptors() const;
        const std::vector<std::int16_t>& links(unsigned room) const;
        std::uint32_t flags(unsigned room) const;

    private:
        const roomlocation::Rooms& rooms;
        doordata::Table portals;
        std::vector<std::string> paths;
        std::vector<std::vector<std::int16_t>> connections;
        std::vector<std::uint32_t> roomAttributes;
        std::array<Resident, 7> residents;
        dooranimation::PoseTables poseTables;
        bool initialized = false;
    };
}
