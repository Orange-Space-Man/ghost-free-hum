#include "resources/dolresources.h"
#include "game/world/roomlocation.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, unsigned offset) {
        return (unsigned(data.at(offset)) << 24) | (unsigned(data.at(offset + 1)) << 16) |
            (unsigned(data.at(offset + 2)) << 8) | data.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    roomlocation::Bounds bounds(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        roomlocation::Bounds result;
        for (unsigned i = 0; i < result.size(); ++i) result[i] = static_cast<std::int32_t>(word(bytes, offset + i * 4));
        return result;
    }
}

roomlocation::Rooms::Rooms(std::vector<Room> records) : records(std::move(records)) {
}

roomlocation::Rooms::Rooms(const std::vector<std::uint8_t>& executable, unsigned map) {
    const auto descriptor = range(executable, word(range(executable, 0x802ff218 + map * 4, 4), 0), 28);
    const auto data = range(executable, word(descriptor, 20), descriptor[0] * 48);
    for (unsigned i = 0; i < descriptor[0]; ++i) {
        const unsigned offset = i * 48;
        Room room{word(data, offset), bounds(data, offset + 8), {}};
        room.ambient = {data[offset + 44], data[offset + 45], data[offset + 46], 255};
        if (data[offset + 32]) {
            const auto regions = range(executable, word(data, offset + 36), data[offset + 32] * 24);
            for (unsigned j = 0; j < data[offset + 32]; ++j) room.regions.push_back(bounds(regions, j * 24));
        }
        records.push_back(std::move(room));
    }
}

bool roomlocation::Rooms::contains(unsigned index, const std::array<float, 3>& position) const {
    const auto& room = records.at(index);
    if (room.regions.empty()) {
        for (unsigned i = 0; i < position.size(); ++i) {
            if (position[i] < static_cast<float>(room.bounds[i]) || static_cast<float>(room.bounds[i + 3]) <= position[i]) return false;
        }
        return true;
    }
    for (const auto& region : room.regions) {
        bool inside = true;
        for (unsigned i = 0; i < position.size(); ++i) {
            if (!(static_cast<float>(region[i]) <= position[i] && position[i] < static_cast<float>(region[i + 3]))) inside = false;
        }
        if (inside) return true;
    }
    return false;
}

unsigned roomlocation::Rooms::resolve(const std::array<float, 3>& position, unsigned previous) const {
    const unsigned hint = previous >> 24;
    if (hint < records.size() && contains(hint, position)) return records[hint].id;
    for (unsigned i = 0; i < records.size(); ++i) if (contains(i, position)) return records[i].id;
    return 0xffffffff;
}

const std::vector<roomlocation::Room>& roomlocation::Rooms::entries() const {
    return records;
}
