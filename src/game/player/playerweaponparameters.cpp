#include "resources/dolresources.h"
#include "game/player/playerweaponparameters.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (std::uint32_t(bytes.at(offset)) << 24) | (std::uint32_t(bytes.at(offset + 1)) << 16) |
            (std::uint32_t(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, std::uint32_t address, std::size_t size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    template<std::size_t Size>
    void overrides(const std::vector<std::uint8_t>& executable, std::uint32_t address, unsigned count,
        std::array<std::uint32_t, Size>& first, std::array<std::uint32_t, Size>& second) {
        if (!count) return;
        if (count > 65535) throw std::runtime_error("Original weapon override count is invalid.");
        const auto bytes = range(executable, address, std::size_t(count) * 12);
        for (unsigned i = 0; i < count; ++i) {
            const auto index = word(bytes, i * 12);
            first.at(index) = word(bytes, i * 12 + 4);
            second.at(index) = word(bytes, i * 12 + 8);
        }
    }
}

std::uint32_t playerweapons::parameterAddress(int actorType, int partType) {
    if (actorType == 0x27) {
        if (partType == 6) return 0x803545f8;
        if (partType == 5) return 0x80354644;
    } else if (actorType == 0x28) {
        if (partType == 6) return 0x80354690;
        if (partType == 5) return 0x803546dc;
    } else if (actorType == 1 || actorType == 0xb3) {
        switch (partType) {
        case 0xb5: return 0x80354b04;
        case 0xb6: return 0x803547c4;
        case 0xb7: return 0x803548ac;
        case 0xb8: return 0x80354994;
        case 0xb9: return 0x80354a7c;
        }
    } else if (actorType >= 0 && actorType <= 4) {
        switch (partType) {
        case 5: return actorType == 4 ? 0x803545ac : 0x80354560;
        case 6: return 0x8035416c;
        case 7: return 0x80354290;
        case 8: return 0x803543b4;
        case 9: return 0x803544d8;
        }
    }
    return 0x80354048;
}

playerweapons::Part::Part(const std::vector<std::uint8_t>& executable, int actorType, int partType) {
    reset(executable, actorType, partType);
}

std::array<std::uint32_t, 19> playerweapons::readParameters(const std::vector<std::uint8_t>& executable, int actorType, int partType) {
    const auto source = range(executable, parameterAddress(actorType, partType), 76);
    std::array<std::uint32_t, 19> result;
    for (unsigned i = 0; i < result.size(); ++i) result[i] = word(source, i * 4);
    return result;
}

void playerweapons::Part::reset(const std::vector<std::uint8_t>& executable, int actorType, int partType) {
    parameters = readParameters(executable, actorType, partType);
    first.fill(parameters[6]);
    second.fill(parameters[7]);
    overrides(executable, parameters[0], parameters[1], first, second);
    field5BC.fill(parameters[6]);
    field5E0.fill(parameters[7]);
    overrides(executable, parameters[4], parameters[5], field5BC, field5E0);
    field574.fill(parameters[6]);
    field598.fill(parameters[7]);
    overrides(executable, parameters[2], parameters[3], field574, field598);
    position = {0, 0, 0};
    field610 = {1, 0, 0};
    field61C = {0, 0, 0};
    field628 = {1, 0, 0};
}
