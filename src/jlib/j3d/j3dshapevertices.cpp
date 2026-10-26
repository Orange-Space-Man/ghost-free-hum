#include "jlib/j3d/j3dshapevertices.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace {
    class Input {
    public:
        explicit Input(const std::vector<std::uint8_t>& bytes) : bytes(bytes) {}
        void range(std::size_t offset, std::size_t length) const {
            if (offset > bytes.size() || length > bytes.size() - offset)
                throw std::runtime_error("J3D vertex data exceeds its resource.");
        }
        unsigned integer(std::size_t offset, unsigned length) const {
            range(offset, length);
            unsigned value = 0;
            for (unsigned i = 0; i < length; ++i) value = (value << 8) | bytes[offset + i];
            return value;
        }
        float scalar(std::size_t offset, unsigned type, unsigned fraction) const {
            const unsigned size = type < 2 ? 1 : type < 4 ? 2 : 4;
            const auto bits = integer(offset, size);
            if (type == 4) {
                float value;
                std::memcpy(&value, &bits, sizeof(value));
                return value;
            }
            int value = static_cast<int>(bits);
            if (type == 1 && value >= 128) value -= 256;
            if (type == 3 && value >= 32768) value -= 65536;
            return std::ldexp(static_cast<float>(value), -static_cast<int>(fraction));
        }
    private:
        const std::vector<std::uint8_t>& bytes;
    };

    struct Format { unsigned count, type, fraction; };
}

std::vector<j3dmodel::ShapePrimitive> j3dmodel::decodeShapeGroup(const Geometry& geometry, const Shape& shape,
    const ShapeGroup& group, bool nbtEnabled) {
    if (!geometry.bytes) throw std::runtime_error("J3D shape has no owned vertex resource.");
    const Input input(*geometry.bytes);
    std::map<unsigned, Format> formats;
    for (std::size_t offset = geometry.vertexFormatOffset;; offset += 16) {
        const auto attribute = input.integer(offset, 4);
        if (attribute == 255) break;
        const Format format{input.integer(offset + 4, 4), input.integer(offset + 8, 4), input.integer(offset + 12, 1)};
        if (attribute < 9 || attribute > 20 || !formats.emplace(attribute, format).second)
            throw std::runtime_error("Invalid J3D vertex format table.");
    }
    std::map<unsigned, std::pair<unsigned, unsigned>> descriptors;
    for (std::size_t offset = shape.descriptorOffset;; offset += 8) {
        const auto attribute = input.integer(offset, 4);
        if (attribute == 255) break;
        const auto mode = input.integer(offset + 4, 4);
        if (mode > 3 || !descriptors.emplace(attribute == 25 ? 10 : attribute, std::make_pair(attribute, mode)).second)
            throw std::runtime_error("Invalid J3D vertex descriptor table.");
    }
    input.range(group.drawOffset, group.drawSize);
    std::size_t cursor = group.drawOffset;
    const std::size_t end = cursor + group.drawSize;
    const auto take = [&](unsigned size) {
        if (cursor > end || size > end - cursor) throw std::runtime_error("Truncated J3D shape command.");
        const auto value = input.integer(cursor, size);
        cursor += size;
        return value;
    };
    std::vector<ShapePrimitive> result;
    while (cursor < end) {
        const auto command = take(1);
        if (!command) continue;
        if ((command & 7) || (command != 0x80 && command != 0x90 && command != 0x98 && command != 0xa0 &&
            command != 0xa8 && command != 0xb0 && command != 0xb8))
            throw std::runtime_error("Unsupported J3D shape primitive command.");
        ShapePrimitive primitive;
        primitive.command = static_cast<std::uint8_t>(command);
        const auto count = take(2);
        primitive.vertices.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            ShapeVertex vertex;
            for (const auto& [order, descriptor] : descriptors) {
                const auto [attribute, mode] = descriptor;
                if (!mode) continue;
                if (attribute <= 8) {
                    if (mode != 1) throw std::runtime_error("J3D matrix indices require direct byte input.");
                    vertex.matrixIndices[attribute] = static_cast<std::uint8_t>(take(1));
                    continue;
                }
                const bool normal = attribute == 10 || attribute == 25;
                if (attribute != 9 && !normal && (attribute < 13 || attribute > 20))
                    throw std::runtime_error("J3D shape attribute requires an additional original vertex decoder.");
                const auto format = formats.at(normal ? 10 : attribute);
                if (format.type > 4 || format.fraction > 31 || format.count > 1)
                    throw std::runtime_error("Unsupported J3D vertex component format.");
                if (attribute == 25 && !nbtEnabled)
                    throw std::runtime_error("J3D NBT descriptor requires the original material NBT binding.");
                const unsigned size = format.type < 2 ? 1 : format.type < 4 ? 2 : 4;
                const unsigned components = normal ? (nbtEnabled || format.count ? 9 : 3) : attribute == 9 ?
                    (format.count ? 3 : 2) : (format.count ? 2 : 1);
                const unsigned stride = normal ? (nbtEnabled ? 9 : 3) * size : components * size;
                std::size_t offset = cursor;
                if (mode == 1) {
                    if (components * size > end - cursor) throw std::runtime_error("Truncated direct J3D vertex.");
                    cursor += components * size;
                } else {
                    const auto index = take(mode == 2 ? 1 : 2);
                    if (attribute == 9 && index == (mode == 2 ? 255U : 65535U)) vertex.skipped = true;
                    const unsigned slot = attribute == 9 ? 0 : normal ? (nbtEnabled ? 2 : 1) : attribute - 8;
                    const auto base = geometry.vertexOffsets.at(slot);
                    if (!base) throw std::runtime_error("J3D vertex array is not bound.");
                    if (vertex.skipped && attribute == 9) continue;
                    offset = std::size_t(base) + std::size_t(index) * stride;
                }
                unsigned fraction = format.fraction;
                if (normal && format.type < 4) fraction = size * 8 - (format.type & 1) - 1;
                for (unsigned component = 0; component < components; ++component) {
                    const auto value = input.scalar(offset + component * size, format.type, fraction);
                    if (attribute == 9) vertex.position[component] = value;
                    else if (normal) vertex.normals[component / 3][component % 3] = value;
                    else vertex.coordinates[attribute - 13][component] = value;
                }
            }
            primitive.vertices.push_back(vertex);
        }
        result.push_back(std::move(primitive));
    }
    return result;
}
