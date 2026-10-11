#include "resources/dolresources.h"
#include "game/scene/gameplayspawn.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "game/startup/startupresources.h"
#include "game/events/jmptooldata.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        if (offset > data.size() || data.size() - offset < 4) throw std::runtime_error("Truncated spawn data.");
        return (unsigned(data[offset]) << 24) | (unsigned(data[offset + 1]) << 16) | (unsigned(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> data;
        std::string error;
        if (!dolresources::read(executable, address, size, data, error)) throw std::runtime_error(error);
        return data;
    }
}

bool gameplayspawn::readCharacter(const std::vector<std::uint8_t>& data, unsigned appearPoint, Spawn& spawn, std::string& error) {
    error.clear();
    try {
        jmp::ToolData table;
        if (!table.attach(data, error)) return false;
        const auto name = table.searchItemInfo("name");
        if (name < 0) throw std::runtime_error("Missing character name field.");
        Spawn result;
        unsigned start = 0;
        while (start < table.count()) {
            const int row = table.findEntryByValue(name, "luige", start);
            if (row < 0) break;
            std::uint32_t point = 0;
            table.getValue(row, "appear_point", point);
            if (point != appearPoint) { start = static_cast<unsigned>(row) + 1; continue; }
            result.characterRecord = row;
            const char* positions[]{"pos_x", "pos_y", "pos_z"};
            for (unsigned axis = 0; axis < 3; ++axis) {
                if (!table.getValue(row, positions[axis], result.position[axis]))
                    throw std::runtime_error("Missing character position field.");
                if (!std::isfinite(result.position[axis])) throw std::runtime_error("Nonfinite spawn position.");
            }
            const char* directions[]{"dir_x", "dir_y", "dir_z"};
            for (unsigned axis = 0; axis < 3; ++axis) {
                float degrees = 0;
                table.getValue(row, directions[axis], degrees);
                if (!std::isfinite(degrees) || std::abs(degrees) > 360000)
                    throw std::runtime_error("Invalid character direction.");
                double angle = degrees;
                while (angle < 0 || 360 < angle) {
                    if (angle < 0) angle += 360;
                    if (360 < angle) angle -= 360;
                }
                const auto rotation = static_cast<unsigned>(65536 * angle / 360) & 65535;
                result.rotation[axis] = static_cast<std::int16_t>(rotation < 32768 ? static_cast<int>(rotation) : static_cast<int>(rotation) - 65536);
            }
            break;
        }
        spawn = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool gameplayspawn::resolveRoom(const std::vector<std::uint8_t>& executable, unsigned map, Spawn& spawn, std::string& error) {
    error.clear();
    try {
        if (map != 2) throw std::runtime_error("Unsupported gameplay spawn map.");
        const auto descriptor = range(executable, word(range(executable, 0x802ff218 + map * 4, 4), 0), 28);
        const auto count = descriptor[0];
        const auto records = range(executable, word(descriptor, 20), count * 48);
        auto position = spawn.position;
        const auto bits = word(range(executable, 0x804d9ab8, 4), 0);
        float height;
        std::memcpy(&height, &bits, 4);
        position[1] += height;
        const auto inside = [&](const std::vector<std::uint8_t>& bounds, unsigned offset) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                const float minimum = static_cast<float>(static_cast<std::int32_t>(word(bounds, offset + axis * 4)));
                const float maximum = static_cast<float>(static_cast<std::int32_t>(word(bounds, offset + 12 + axis * 4)));
                if (!(position[axis] >= minimum && position[axis] < maximum)) return false;
            }
            return true;
        };
        for (unsigned i = 0; i < count; ++i) {
            const auto offset = i * 48;
            bool found = false;
            const auto regions = records[offset + 32];
            if (!regions) found = inside(records, offset + 8);
            else {
                const auto bounds = range(executable, word(records, offset + 36), regions * 24);
                for (unsigned region = 0; region < regions; ++region) if (inside(bounds, region * 24)) { found = true; break; }
            }
            if (!found) continue;
            const auto roomId = word(records, offset);
            const auto paths = range(executable, word(descriptor, 4), count * 4);
            const auto room = roomId & 255;
            if (room >= count) throw std::runtime_error("Room resource index out of range.");
            const auto path = range(executable, word(paths, room * 4), 128);
            const auto end = std::find(path.begin(), path.end(), 0);
            if (end == path.end()) throw std::runtime_error("Unterminated room resource path.");
            spawn.roomId = roomId;
            spawn.roomArchive.assign(path.begin(), end);
            return true;
        }
        throw std::runtime_error("Spawn does not resolve to a room.");
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool gameplayspawn::load(const std::filesystem::path& path, Spawn& spawn, roomscene::Scene& room, std::string& error) {
    gameassets::Package package;
    std::vector<std::uint8_t> data, archive, executable;
    Spawn result;
    roomscene::Scene loaded;
    if (!package.open(path, error) || !package.read("files/Map/map2.szp", data, error) ||
        !roomarchive::decompress(data, archive, error) || !roomarchive::read(archive, "jmp/characterinfo", data, error) ||
        !readCharacter(data, 0, result, error) || !package.read("sys/main.dol", executable, error) ||
        !resolveRoom(executable, 2, result, error)) return false;
    const auto number = result.roomId & 255;
    const auto expected = "/Iwamoto/map2/room_" + std::string(number < 10 ? "0" : "") + std::to_string(number) + ".arc";
    if (result.roomArchive != expected) { error = "Room resource differs from the supported map loader."; return false; }
    if (!roomscene::read(path, loaded, error, number)) return false;
    spawn = std::move(result);
    room = std::move(loaded);
    return true;
}
