#include "game/models/modelvertexstream.h"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    void range(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("Model vertex stream exceeds its resource.");
    }

    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        range(bytes, offset, 2);
        return (unsigned(bytes[offset]) << 8) | bytes[offset + 1];
    }

    std::uint32_t word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (std::uint32_t(half(bytes, offset)) << 16) | half(bytes, offset + 2);
    }

    float real(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const auto bits = word(bytes, offset);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

bool modeldraw::decodeVertices(const std::vector<std::uint8_t>& model, const Batch& batch,
    std::vector<Primitive>& primitives, std::string& error) {
    error.clear();
    try {
        const bool floatPositions = (word(model, 0x48) & 2) != 0;
        const auto positions = word(model, 0x14), normals = word(model, 0x18), colors = word(model, 0x1c);
        std::array<std::uint32_t, 4> coordinates;
        for (unsigned i = 0; i < 4; ++i) coordinates[i] = word(model, 0x24 + i * 4);
        const auto& bytes = batch.displayList;
        std::size_t cursor = 0;
        const auto index = [&]() {
            const auto value = half(bytes, cursor);
            cursor += 2;
            return value;
        };
        const auto arrayOffset = [&](std::uint32_t base, unsigned element, unsigned stride) {
            if (!base) throw std::runtime_error("Missing model vertex attribute array.");
            const auto offset = std::size_t(base) + std::size_t(element) * stride;
            range(model, offset, stride);
            return offset;
        };
        std::vector<Primitive> result;
        while (cursor < bytes.size()) {
            const auto command = bytes[cursor++];
            if (!command) continue;
            if ((command & 7) || (command != 0x80 && command != 0x90 && command != 0x98 && command != 0xa0 &&
                command != 0xa8 && command != 0xb0 && command != 0xb8))
                throw std::runtime_error("Unsupported model display-list command.");
            Primitive primitive;
            primitive.command = command;
            const auto count = index();
            primitive.vertices.reserve(count);
            for (unsigned i = 0; i < count; ++i) {
                Vertex vertex;
                auto offset = arrayOffset(positions, index(), floatPositions ? 12 : 6);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    if (floatPositions) vertex.position[axis] = real(model, offset + axis * 4);
                    else {
                        const auto value = half(model, offset + axis * 2);
                        vertex.position[axis] = float(value < 0x8000 ? int(value) : int(value) - 0x10000);
                    }
                }
                const unsigned normalCount = batch.normalMode ? 3 : ((batch.attributes & 0x400) ? 1 : 0);
                for (unsigned normal = 0; normal < normalCount; ++normal) {
                    offset = arrayOffset(normals, index(), 12);
                    for (unsigned axis = 0; axis < 3; ++axis) vertex.normals[normal][axis] = real(model, offset + axis * 4);
                }
                if (batch.attributes & 0x800) {
                    offset = arrayOffset(colors, index(), 4);
                    for (unsigned channel = 0; channel < 4; ++channel) vertex.color[channel] = model[offset + channel];
                }
                for (unsigned coordinate = 0; coordinate < 4; ++coordinate) {
                    if (!(batch.attributes & (0x2000U << coordinate))) continue;
                    offset = arrayOffset(coordinates[coordinate], index(), 8);
                    for (unsigned axis = 0; axis < 2; ++axis)
                        vertex.coordinates[coordinate][axis] = real(model, offset + axis * 4);
                }
                primitive.vertices.push_back(vertex);
            }
            result.push_back(std::move(primitive));
        }
        primitives = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
