#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/enemies/enemytypecatalog.h"
#include "game/startup/startupresources.h"

#include <stdexcept>
#include <utility>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
            (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
    }

    std::uint32_t hash(std::string_view name) {
        std::uint32_t result = 0;
        for (unsigned char byte : name) {
            if (!byte) break;
            const int value = byte < 128 ? byte : int(byte) - 256;
            result = (result * 256 + static_cast<std::uint32_t>(value)) % 33554393;
        }
        return result;
    }
}

bool enemytypes::Catalog::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        std::vector<std::uint8_t> table;
        if (!dolresources::read(executable, glme01::actorTypeTable, 381 * 28, table, error)) return false;
        Catalog result;
        for (unsigned i = 0; i < result.types.size(); ++i) {
            auto& type = result.types[i];
            type.flags = static_cast<std::uint16_t>(word(table, i * 28 + 8) >> 16);
            const auto address = word(table, i * 28 + 24);
            for (unsigned offset = 0; ; ++offset) {
                std::vector<std::uint8_t> byte;
                if (address > UINT32_MAX - offset ||
                    !dolresources::read(executable, address + offset, 1, byte, error))
                    throw std::runtime_error("Invalid enemy type name address.");
                if (!byte[0]) break;
                type.name.push_back(static_cast<char>(byte[0]));
            }
            type.hash = hash(type.name);
        }
        result.loaded = true;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::uint32_t enemytypes::Catalog::find(std::string_view name) const {
    if (!loaded) throw std::runtime_error("Enemy type catalog is not loaded.");
    name = name.substr(0, name.find('\0'));
    const auto key = hash(name);
    for (unsigned i = 0; i < types.size(); ++i) {
        if (types[i].hash == key && types[i].name == name) return i;
    }
    return 0xffffffff;
}

const std::string& enemytypes::Catalog::name(unsigned type) const {
    if (!loaded) throw std::runtime_error("Enemy type catalog is not loaded.");
    return types.at(type).name;
}

std::uint16_t enemytypes::Catalog::flags(unsigned type) const {
    if (!loaded) throw std::runtime_error("Enemy type catalog is not loaded.");
    return types.at(type).flags;
}
