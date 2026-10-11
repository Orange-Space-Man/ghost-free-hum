#include "resources/dolresources.h"
#include "game/world/roomdependencies.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> read(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }
}

roomdependencies::Catalog::Catalog(const std::vector<std::uint8_t>& executable, unsigned map) {
    if (map > (0xffffffffU - 0x802ff218U) / 4) throw std::out_of_range("Map descriptor address overflows.");
    const auto descriptor = read(executable, word(read(executable, 0x802ff218 + map * 4, 4), 0), 28);
    const auto pointers = read(executable, word(descriptor, 8), descriptor.at(0) * 4);
    rooms.resize(descriptor.at(0));
    for (unsigned room = 0; room < rooms.size(); ++room) {
        auto address = word(pointers, room * 4);
        for (;;) {
            const auto bytes = read(executable, address, 2);
            const unsigned raw = (unsigned(bytes[0]) << 8) | bytes[1];
            const auto value = static_cast<std::int16_t>(raw);
            if (value == -1) break;
            rooms[room].push_back(value);
            if (address > 0xfffffffd) throw std::runtime_error("Original room dependency list overflows its address.");
            address += 2;
        }
    }
}

const std::vector<std::int16_t>* roomdependencies::Catalog::list(unsigned room) const {
    const unsigned index = room & 255;
    return index == 255 ? nullptr : &rooms.at(index);
}

bool roomdependencies::Catalog::contains(unsigned playerRoom, unsigned room) const {
    const auto* dependencies = list(playerRoom);
    if (!dependencies) throw std::runtime_error("Original player room has no dependency list.");
    return std::any_of(dependencies->begin(), dependencies->end(), [room](std::int16_t entry) {
        return static_cast<unsigned>(static_cast<int>(entry)) == room;
    });
}
