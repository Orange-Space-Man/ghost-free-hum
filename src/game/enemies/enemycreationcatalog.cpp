#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/enemies/enemycreationcatalog.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) {
            throw std::runtime_error(error);
        }
        return result;
    }
}

bool enemycreation::Catalog::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        const auto pools = range(executable, glme01::enemyCreationPools, 32);
        const auto counts = range(executable, glme01::enemyCreationCounts, 16);
        const auto typeTable = range(executable, glme01::actorTypeTable, 381 * 28);
        const auto constructors = range(executable, glme01::enemyConstructors, 381 * 16);
        const auto models = range(executable, glme01::actorAuxiliaryTable, 262 * 64);
        Catalog result;
        for (unsigned i = 0; i < 4; ++i) {
            result.groups[i] = {word(pools, i * 8), word(pools, i * 8 + 4), word(counts, i * 4)};
        }
        for (unsigned i = 0; i < 381; ++i) {
            auto& type = result.types[i];
            type.model = word(typeTable, i * 28);
            type.constructor = word(constructors, i * 16);
            const unsigned group = word(typeTable, i * 28 + 20) >> 16;
            type.group = static_cast<std::int16_t>(group < 32768 ? int(group) : int(group) - 65536);
            type.validModel = type.model < 262;
            if (type.validModel) {
                type.modelCapacity = word(models, type.model * 64);
            }
        }
        result.loaded = true;
        *this = result;
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const std::array<enemycreation::Pool, 4>& enemycreation::Catalog::pools() const {
    if (!loaded) {
        throw std::runtime_error("Actor creation catalog is not loaded.");
    }
    return groups;
}

unsigned enemycreation::Catalog::category(unsigned type) const {
    if (!loaded || !types.at(type).validModel) {
        throw std::runtime_error("Actor type has no valid original model capacity.");
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (types[type].modelCapacity <= groups[i].capacity) {
            return i;
        }
    }
    return 0xffffffff;
}

unsigned enemycreation::Catalog::capacity(unsigned type) const {
    const auto index = category(type);
    if (index == 0xffffffff) {
        throw std::runtime_error("Original actor capacity lookup would index before its pool table.");
    }
    return groups[index].capacity;
}

unsigned enemycreation::Catalog::model(unsigned type) const {
    if (!loaded) {
        throw std::runtime_error("Actor creation catalog is not loaded.");
    }
    return types.at(type).model;
}

std::uint32_t enemycreation::Catalog::ownerConstructor(unsigned type) const {
    if (!loaded) {
        throw std::runtime_error("Actor creation catalog is not loaded.");
    }
    return types.at(type).constructor;
}

std::int16_t enemycreation::Catalog::group(unsigned type) const {
    if (!loaded) throw std::runtime_error("Actor creation catalog is not loaded.");
    return types.at(type).group;
}

bool enemycreation::Catalog::suppressInClearedRoom(unsigned type) const {
    if (!loaded) {
        throw std::runtime_error("Actor creation catalog is not loaded.");
    }
    return types.at(type).group != -1;
}

bool enemycreation::Catalog::countsForRoom(unsigned type) const {
    if (!loaded) {
        throw std::runtime_error("Actor creation catalog is not loaded.");
    }
    const auto group = types.at(type).group;
    return !(type >= 0xef && type <= 0xfd) && group != -1 && group != 1;
}
