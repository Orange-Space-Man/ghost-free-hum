#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/enemies/enemystrategycatalog.h"
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
        if (!dolresources::read(executable, address, size, result, error)) {
            throw std::runtime_error(error);
        }
        return result;
    }
}

bool enemystrategy::Catalog::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        Catalog result;
        const auto types = range(executable, glme01::strategyTypes, 381 * 16);
        const auto factories = range(executable, glme01::strategyFactories, 187 * 4);
        for (unsigned i = 0; i < result.constructors.size(); ++i) {
            result.constructors[i] = word(factories, i * 4);
        }
        for (unsigned i = 0; i < result.selectors.size(); ++i) {
            result.attachmentJoints[i] = word(types, i * 16 + 12);
            const auto generatorAddress = word(types, i * 16 + 4);
            const auto generatorCode = range(executable, generatorAddress, 36);
            unsigned generatorDisplacement = word(generatorCode, 16) & 0x03fffffc;
            if (generatorDisplacement & 0x02000000) generatorDisplacement |= 0xfc000000;
            if (word(generatorCode, 0) != 0x7c0802a6 || (word(generatorCode, 4) & 0xffff0000) != 0x38800000 ||
                word(generatorCode, 8) != 0x90010004 || word(generatorCode, 12) != 0x9421fff8 ||
                (word(generatorCode, 16) & 0xfc000003) != 0x48000001 || generatorAddress + 16 + generatorDisplacement != 0x800db168 ||
                word(generatorCode, 20) != 0x8001000c || word(generatorCode, 24) != 0x38210008 ||
                word(generatorCode, 28) != 0x7c0803a6 || word(generatorCode, 32) != 0x4e800020) {
                throw std::runtime_error("Unsupported original generator strategy selector.");
            }
            result.generatorSelectors[i] = word(generatorCode, 4) & 0xffff;
            if (result.generatorSelectors[i] >= result.constructors.size()) {
                throw std::runtime_error("Original generator selector exceeds its constructor table.");
            }
            const unsigned address = word(types, i * 16);
            const auto first = range(executable, address, 8);
            auto& selector = result.selectors[i];
            if ((word(first, 0) & 0xffff0000) == 0x38600000 && word(first, 4) == 0x4e800020) {
                selector.strategy = word(first, 0) & 0xffff;
            } else {
                const auto code = range(executable, address, 36);
                const auto branch = word(code, 16);
                unsigned displacement = branch & 0x03fffffc;
                if (displacement & 0x02000000) {
                    displacement |= 0xfc000000;
                }
                if (word(code, 0) != 0x7c0802a6 || (word(code, 4) & 0xffff0000) != 0x38800000 ||
                    word(code, 8) != 0x90010004 || word(code, 12) != 0x9421fff8 ||
                    (branch & 0xfc000003) != 0x48000001 || address + 16 + displacement != 0x800db0d8 ||
                    word(code, 20) != 0x8001000c || word(code, 24) != 0x38210008 ||
                    word(code, 28) != 0x7c0803a6 || word(code, 32) != 0x4e800020) {
                    throw std::runtime_error("Unsupported original enemy strategy selector.");
                }
                selector.strategy = word(code, 4) & 0xffff;
                selector.attackOverride = true;
            }
            if (selector.strategy >= result.constructors.size()) {
                throw std::runtime_error("Original enemy strategy selector exceeds its constructor table.");
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

unsigned enemystrategy::Catalog::select(unsigned type, const jmp::ToolDataRef& source) const {
    if (!loaded) {
        throw std::runtime_error("Enemy strategy catalog is not loaded.");
    }
    const auto& selector = selectors.at(type);
    if (selector.attackOverride && source.table && source.row >= 0) {
        std::uint32_t attackType = 0;
        source.table->getValue(source.row, "attack_type", attackType);
        if (attackType == 3) {
            return 0;
        }
    }
    return selector.strategy;
}

std::uint32_t enemystrategy::Catalog::constructor(unsigned strategy) const {
    if (!loaded) {
        throw std::runtime_error("Enemy strategy catalog is not loaded.");
    }
    return constructors.at(strategy);
}

unsigned enemystrategy::Catalog::selectGenerator(unsigned type) const {
    if (!loaded) throw std::runtime_error("Enemy strategy catalog is not loaded.");
    return generatorSelectors.at(type);
}

unsigned enemystrategy::Catalog::attachmentJoint(unsigned type) const {
    if (!loaded) throw std::runtime_error("Enemy strategy catalog is not loaded.");
    return attachmentJoints.at(type);
}
