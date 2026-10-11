#include "resources/glme01addresses.h"
#include "game/world/roomdoorplacement.h"

#include <stdexcept>

namespace {
    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        if (offset > data.size() || data.size() - offset < 4) throw std::runtime_error("Truncated executable data.");
        return (static_cast<std::uint32_t>(data[offset]) << 24) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::size_t offset(const std::vector<std::uint8_t>& data, std::uint32_t address, std::size_t length) {
        for (unsigned index = 0; index < 18; ++index) {
            const auto start = read32(data, 0x48 + index * 4);
            const auto size = read32(data, 0x90 + index * 4);
            if (address < start || address - start >= size || length > size - (address - start)) continue;
            const auto result = static_cast<std::size_t>(read32(data, index * 4)) + address - start;
            if (result <= data.size() && length <= data.size() - result) return result;
        }
        throw std::runtime_error("Door data address lies outside executable sections.");
    }

    float signed32(std::uint32_t value) {
        return static_cast<float>(value < 0x80000000U ? static_cast<std::int64_t>(value) : static_cast<std::int64_t>(value) - 0x100000000LL);
    }
}

bool roomdoors::read(const std::vector<std::uint8_t>& executable, unsigned room,
    std::vector<roomobjects::Placement>& placements, std::string& error) {
    placements.clear();
    error.clear();
    try {
        // GLME01 revision 0: map descriptors, door model configuration, and model paths.
        constexpr std::uint32_t mapPointers = 0x802ff218;
        constexpr std::uint32_t doorConfigurations = glme01::doorConfigurations;
        constexpr std::uint32_t doorModels = glme01::doorModels;
        const auto pointer = [&](std::uint32_t address) { return read32(executable, offset(executable, address, 4)); };
        const auto map = pointer(mapPointers + 2 * 4);
        const auto doors = pointer(map + 0x18);
        std::vector<roomobjects::Placement> result;
        for (unsigned index = 0; index < 256; ++index) {
            const auto record = offset(executable, doors + index * 28, 28);
            const auto orientation = executable[record];
            if (orientation == 0) {
                placements = std::move(result);
                return true;
            }
            if (executable[record + 26] != room && executable[record + 27] != room) continue;
            const auto identity = read32(executable, record + 4);
            if (identity == 0xff) continue;
            const auto model = (identity >> 8) & 255;
            if (model == 0 || model > 14 || (orientation != 1 && orientation != 2)) throw std::runtime_error("Unsupported door record.");
            const auto configuration = offset(executable, doorConfigurations + model * 20, 20);
            const auto leaves = executable[configuration];
            if (leaves != 1 && leaves != 2) throw std::runtime_error("Invalid door leaf count.");
            const auto name = pointer(doorModels + model * 4);
            std::string path;
            for (unsigned character = 0; character < 128; ++character) {
                auto value = executable[offset(executable, name + character, 1)];
                if (value == 0) break;
                if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
                path += static_cast<char>(value);
            }
            if (path.empty() || path.size() == 128 || path.front() != '/') throw std::runtime_error("Invalid door model path.");
            path.erase(0, 1);
            const auto heightBits = (executable[record + 22] << 8) | executable[record + 23];
            const float height = static_cast<float>(heightBits < 0x8000 ? heightBits : heightBits - 0x10000);
            for (unsigned leaf = 0; leaf < leaves; ++leaf) {
                roomobjects::Placement placement;
                placement.model = path;
                const float sign = leaf == 0 ? 1.0f : -1.0f;
                for (unsigned axis = 0; axis < 3; ++axis) {
                    const unsigned offsetAxis = orientation == 2 && axis != 1 ? 2 - axis : axis;
                    placement.position[axis] = signed32(read32(executable, record + 8 + axis * 4)) +
                        sign * signed32(read32(executable, configuration + 4 + offsetAxis * 4));
                }
                placement.position[1] -= height * 0.5f;
                placement.rotation[1] = (orientation == 2 ? -90.0f : 0.0f) + (leaf == 0 ? 0.0f : 180.0f);
                result.push_back(std::move(placement));
            }
        }
        throw std::runtime_error("Missing door table terminator.");
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
