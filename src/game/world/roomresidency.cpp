#include "resources/dolresources.h"
#include "game/world/roomresidency.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> read(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }
}

roomresidency::InitialRooms::InitialRooms(const std::vector<std::uint8_t>& executable, unsigned map,
    const roomlocation::Rooms& rooms) : rooms(rooms) {
    const auto descriptor = read(executable, word(read(executable, 0x802ff218 + map * 4, 4), 0), 28);
    const auto count = descriptor[0];
    if (count != rooms.entries().size()) throw std::runtime_error("Room residency descriptor does not match its room owner.");
    const auto records = read(executable, word(descriptor, 20), count * 48);
    const auto strings = read(executable, word(descriptor, 4), count * 4);
    std::string error;
    if (!poseTables.read(executable, error)) throw std::runtime_error(error);
    if (!doordata::read(executable, word(descriptor, 24), portals, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < count; ++i) {
        roomAttributes.push_back(word(records, i * 48 + 4));
        std::string path;
        const unsigned address = word(strings, i * 4);
        for (unsigned offset = 0;; ++offset) {
            if (offset == 1024) throw std::runtime_error("Unterminated original room path.");
            const auto character = read(executable, address + offset, 1)[0];
            if (!character) break;
            path.push_back(static_cast<char>(character));
        }
        paths.push_back(std::move(path));
        auto link = word(records, i * 48 + 40);
        std::vector<std::int16_t> entries;
        for (;;) {
            const auto bytes = read(executable, link, 2);
            const auto index = static_cast<std::int16_t>((unsigned(bytes[0]) << 8) | bytes[1]);
            if (index == -1) break;
            if (index < 0 || unsigned(index) >= portals.entries.size()) throw std::runtime_error("Original room portal index is invalid.");
            entries.push_back(index);
            if (entries.size() > portals.entries.size()) throw std::runtime_error("Unterminated original room portal list.");
            link += 2;
        }
        connections.push_back(std::move(entries));
    }
}

std::vector<unsigned> roomresidency::InitialRooms::required(const std::vector<std::uint32_t>& playerRooms) const {
    std::vector<unsigned> result;
    for (const auto id : playerRooms) {
        const auto index = id >> 24;
        if (rooms.entries().at(index).id != id) throw std::runtime_error("Room residency requires a resolved player room.");
        result.push_back(id & 255);
        for (const auto link : connections.at(index)) {
            const auto& portal = portals.entries.at(link);
            if (portal.identity != 0xff) continue;
            const auto neighbor = portal.rooms[0] == index ? portal.rooms[1] : portal.rooms[0];
            const auto room = rooms.entries().at(neighbor).id & 255;
            if (std::find(result.begin(), result.end(), room) == result.end()) result.push_back(room);
        }
    }
    return result;
}

void roomresidency::InitialRooms::load(gameassets::Package& package, const std::vector<std::uint32_t>& playerRooms,
    const jmp::ToolData& furniture, const gameflags::State& flags) {
    if (initialized) throw std::runtime_error("Initial room loading cannot replace an active room lifecycle.");
    const auto requests = required(playerRooms);
    std::array<Resident, 7> result;
    std::string error;
    for (unsigned i = 0; i < requests.size() && i < result.size(); ++i) {
        auto& resident = result[i];
        resident.room = requests[i];
        resident.path = paths.at(resident.room);
        auto path = resident.path;
        if (!path.empty() && path.front() == '/') path.erase(path.begin());
        std::vector<std::uint8_t> bytes;
        if (!package.read("files/" + path, bytes, error)) throw std::runtime_error(error);
        const auto extension = path.substr(path.find_last_of('.'));
        if (extension == ".arc") {
            if (!resident.archive.read(bytes, error)) throw std::runtime_error(error);
            const auto resource = resident.archive.findPathResource("/room.bin");
            if (resource < 0) throw std::runtime_error("Original room archive has no room.bin resource.");
            resident.model = resident.archive.resource(resource);
        } else if (extension == ".bin") resident.model = std::move(bytes);
        else throw std::runtime_error("Unsupported original room resource extension.");
        if (!resident.drawing.read(resident.model, poseTables, error)) throw std::runtime_error(resident.path + ": " + error);
        if (extension == ".arc") resident.furniture.load(resident.archive, furniture, resident.room, flags, poseTables);
    }
    residents = std::move(result);
    initialized = true;
}

const std::array<roomresidency::Resident, 7>& roomresidency::InitialRooms::slots() const { return residents; }
const doordata::Table& roomresidency::InitialRooms::descriptors() const { return portals; }
const std::vector<std::int16_t>& roomresidency::InitialRooms::links(unsigned room) const { return connections.at(room); }
std::uint32_t roomresidency::InitialRooms::flags(unsigned room) const { return roomAttributes.at(room); }
