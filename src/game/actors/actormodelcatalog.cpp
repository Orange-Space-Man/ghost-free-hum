#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/actors/actormodelcatalog.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) {
            throw std::runtime_error(error);
        }
        return bytes;
    }

    template<std::size_t Width>
    std::vector<std::array<std::uint32_t, Width>> records(const std::vector<std::uint8_t>& executable,
        unsigned address, unsigned count) {
        if (count > executable.size() / (Width * 4)) {
            throw std::runtime_error("Actor animation table exceeds executable bounds.");
        }
        std::vector<std::array<std::uint32_t, Width>> result(count);
        if (!count) {
            return result;
        }
        const auto bytes = range(executable, address, static_cast<unsigned>(count * Width * 4));
        for (unsigned i = 0; i < count; ++i) {
            for (unsigned field = 0; field < Width; ++field) {
                result[i][field] = word(bytes, static_cast<unsigned>((i * Width + field) * 4));
            }
        }
        return result;
    }
}

bool actormodel::Catalog::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        Catalog result;
        const auto modelTable = range(executable, glme01::actorModelTable, 262 * 52);
        const auto auxiliaryTable = range(executable, glme01::actorAuxiliaryTable, 262 * 64);
        const auto keyTable = range(executable, glme01::actorKeyTable, 262 * 8);
        const auto typeTable = range(executable, glme01::actorTypeTable, 381 * 28);
        const auto animationTable = range(executable, glme01::actorAnimationTable, 381 * 8);
        const std::array<unsigned, 5> auxiliaryOffsets{32, 16, 40, 48, 56};
        for (unsigned i = 0; i < result.models.size(); ++i) {
            auto& model = result.models[i];
            const auto pathAddress = word(modelTable, i * 52);
            for (unsigned offset = 0; offset < 256; ++offset) {
                const auto character = range(executable, pathAddress + offset, 1).front();
                if (!character) {
                    break;
                }
                model.path.push_back(static_cast<char>(character));
            }
            if (model.path.empty() || model.path.size() == 256) {
                throw std::runtime_error("Invalid original actor archive path.");
            }
            model.layout.keys = word(modelTable, i * 52 + 12);
            model.capacity = word(auxiliaryTable, i * 64);
            for (unsigned group = 0; group < auxiliaryOffsets.size(); ++group) {
                model.layout.auxiliary[group] = word(auxiliaryTable, i * 64 + auxiliaryOffsets[group]);
            }
            model.keyMetadata = records<6>(executable, word(keyTable, i * 8 + 4), word(keyTable, i * 8));
        }
        for (unsigned i = 0; i < result.types.size(); ++i) {
            auto& type = result.types[i];
            type.model = word(typeTable, i * 28);
            type.field4 = static_cast<std::uint16_t>(word(typeTable, i * 28 + 4) >> 16);
            type.field14 = static_cast<std::int16_t>(word(typeTable, i * 28 + 20) >> 16);
            type.flags = word(typeTable, i * 28 + 12);
            const auto texture = word(typeTable, i * 28 + 16);
            type.texture = texture <= 0x7fffffff ? static_cast<std::int32_t>(texture) :
                static_cast<std::int32_t>(std::int64_t(texture) - 0x100000000LL);
            type.animations = records<8>(executable, word(animationTable, i * 8 + 4), word(animationTable, i * 8));
            for (const auto& animation : type.animations) {
                std::string name;
                for (unsigned offset = 0; offset < 256; ++offset) {
                    const auto character = range(executable, animation[7] + offset, 1).front();
                    if (!character) break;
                    name.push_back(static_cast<char>(character));
                }
                if (name.size() == 256) throw std::runtime_error("Unterminated original actor animation name.");
                type.animationNames.push_back(std::move(name));
            }
        }
        result.loaded = true;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const actormodel::ModelInfo& actormodel::Catalog::model(unsigned index) const {
    if (!loaded) {
        throw std::runtime_error("Actor model catalog is not loaded.");
    }
    return models.at(index);
}

const actormodel::TypeInfo& actormodel::Catalog::type(unsigned index) const {
    if (!loaded) {
        throw std::runtime_error("Actor model catalog is not loaded.");
    }
    return types.at(index);
}

int actormodel::Catalog::findAnimation(unsigned index, std::string_view name) const {
    name = name.substr(0, name.find('\0'));
    const auto& names = type(index).animationNames;
    for (unsigned i = 0; i < names.size(); ++i) if (names[i] == name) return static_cast<int>(i);
    return -1;
}
