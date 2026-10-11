#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/world/doordescriptors.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
            (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
    }

    std::int32_t signedWord(std::uint32_t bits) {
        std::int32_t result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

bool doordata::read(const std::vector<std::uint8_t>& executable, std::uint32_t address,
    Table& table, std::string& error) {
    error.clear();
    try {
        Table result;
        for (;;) {
            std::vector<std::uint8_t> bytes;
            if (!dolresources::read(executable, address, 1, bytes, error)) return false;
            if (!bytes[0]) break;
            if (!dolresources::read(executable, address, 28, bytes, error)) return false;
            Descriptor entry;
            std::copy_n(bytes.begin(), 4, entry.attributes.begin());
            entry.identity = word(bytes, 4);
            for (unsigned axis = 0; axis < 3; ++axis) {
                entry.center[axis] = signedWord(word(bytes, 8 + axis * 4));
                const int value = (int(bytes[20 + axis * 2]) << 8) | bytes[21 + axis * 2];
                entry.dimensions[axis] = static_cast<std::int16_t>(value < 32768 ? value : value - 65536);
            }
            entry.rooms = {bytes[26], bytes[27]};
            if (entry.identity != 0xff) {
                std::vector<std::uint8_t> configuration;
                const auto type = (entry.identity >> 8) & 0xff;
                if (!dolresources::read(executable, glme01::doorConfigurations + type * 20, 1, configuration, error)) return false;
                result.liveCount = static_cast<std::uint16_t>(result.liveCount + (configuration[0] == 2 ? 2 : 1));
            }
            result.entries.push_back(entry);
            if (address > UINT32_MAX - 28) throw std::runtime_error("Door descriptor address overflow.");
            address += 28;
        }
        table = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool doordata::traversalEnabled(const Descriptor& descriptor) {
    return (descriptor.attributes[0] & 1) != 0;
}
