#include "game/world/roomobjectplacement.h"

#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace {
    struct Field {
        std::uint32_t mask = 0;
        unsigned offset = 0;
        unsigned shift = 0;
        unsigned type = 0;
    };

    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 4, "Truncated furniture table.");
        return (static_cast<std::uint32_t>(data[offset]) << 24) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::uint32_t fieldHash(const char* name) {
        std::uint32_t value = 0;
        while (*name) value = ((value << 8) + static_cast<unsigned char>(*name++)) % 33554393;
        return value;
    }
}

bool roomobjects::read(const std::vector<std::uint8_t>& data, unsigned room, std::vector<Placement>& placements, std::string& error) {
    placements.clear();
    error.clear();
    try {
        const std::size_t count = read32(data, 0);
        const std::size_t fieldCount = read32(data, 4);
        const std::size_t records = read32(data, 8);
        const std::size_t stride = read32(data, 12);
        require(fieldCount <= (data.size() - 16) / 12 && records >= 16 + fieldCount * 12 && records <= data.size() &&
            stride > 0 && count <= (data.size() - records) / stride && count <= 100000, "Invalid furniture table ranges.");
        std::map<std::uint32_t, Field> fields;
        for (std::size_t index = 0; index < fieldCount; ++index) {
            const auto offset = 16 + index * 12;
            const auto packed = read32(data, offset + 8);
            Field field{read32(data, offset + 4), packed >> 16, (packed >> 8) & 255, packed & 255};
            const unsigned size = field.type == 1 ? 32 : 4;
            require(field.type <= 2 && field.shift < 32 && field.offset <= stride && size <= stride - field.offset,
                "Invalid furniture field layout.");
            require(fields.emplace(read32(data, offset), field).second, "Duplicate furniture field.");
        }
        const auto field = [&](const char* name, unsigned type) -> const Field& {
            const auto found = fields.find(fieldHash(name));
            require(found != fields.end() && found->second.type == type, "Missing required furniture field.");
            return found->second;
        };
        const auto& roomField = field("room_no", 0);
        const auto& modelField = field("dmd_name", 1);
        const char* positions[]{"pos_x", "pos_y", "pos_z"};
        const char* rotations[]{"dir_x", "dir_y", "dir_z"};
        const char* scales[]{"scale_x", "scale_y", "scale_z"};
        for (unsigned axis = 0; axis < 3; ++axis) {
            field(positions[axis], 2);
            field(rotations[axis], 2);
            field(scales[axis], 2);
        }
        std::vector<Placement> result;
        for (std::size_t index = 0; index < count; ++index) {
            const auto record = records + index * stride;
            if (((read32(data, record + roomField.offset) & roomField.mask) >> roomField.shift) != room) continue;
            Placement placement;
            for (unsigned character = 0; character < 32; ++character) {
                const auto value = data[record + modelField.offset + character];
                if (value == 0) break;
                placement.model += static_cast<char>(value);
            }
            if (placement.model.empty() || placement.model == "(null)") continue;
            require(placement.model.size() < 32, "Unterminated furniture model name.");
            for (const unsigned char character : placement.model) {
                require((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') || character == '_' || character == '-', "Invalid furniture model name.");
            }
            const auto number = [&](const char* name) {
                const auto bits = read32(data, record + field(name, 2).offset);
                float value;
                std::memcpy(&value, &bits, sizeof(value));
                require(std::isfinite(value), "Non-finite furniture transform.");
                return value;
            };
            for (unsigned axis = 0; axis < 3; ++axis) {
                placement.position[axis] = number(positions[axis]);
                placement.rotation[axis] = number(rotations[axis]);
                placement.scale[axis] = number(scales[axis]);
            }
            result.push_back(std::move(placement));
        }
        placements = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

roomgeometry::Position roomobjects::transform(const roomgeometry::Position& position, const Placement& placement) {
    roomgeometry::Position result{};
    for (unsigned axis = 0; axis < 3; ++axis) result[axis] = position[axis] * placement.scale[axis];
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float angle = placement.rotation[axis] * 0.01745329252f;
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const unsigned a = (axis + 1) % 3;
        const unsigned b = (axis + 2) % 3;
        const float first = result[a];
        result[a] = cosine * first - sine * result[b];
        result[b] = sine * first + cosine * result[b];
    }
    for (unsigned axis = 0; axis < 3; ++axis) result[axis] += placement.position[axis];
    return result;
}
