#include "game/effects/particlebillboards.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cstring>
#include <cmath>
#include <stdexcept>

namespace {
    float floating(std::uint32_t bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::uint32_t word(const particles::Resource& source, unsigned offset) {
        return (unsigned(source->at(offset)) << 24) | (unsigned(source->at(offset + 1)) << 16) |
            (unsigned(source->at(offset + 2)) << 8) | source->at(offset + 3);
    }

    float phase(const particles::DrawState& draw, const particles::Node& node) {
        if (draw.fields18[2]) return float(int(node.fields30[1]) - 1) / node.fields30[0];
        const unsigned cycle = draw.fields18[1];
        if (!cycle) throw std::runtime_error("Particle color cycle has zero duration.");
        auto age = static_cast<std::uint16_t>(unsigned(node.field34) + node.fields30[1] - 1);
        while (age > cycle) age = static_cast<std::uint16_t>(age - cycle);
        return float(age) / cycle;
    }

    std::array<std::uint8_t, 4> color(const particles::DrawState& draw, unsigned array, float time) {
        if (draw.count < 2) throw std::runtime_error("Particle color curve has fewer than two keys.");
        unsigned next = 1;
        while (next < draw.count && floating(word(draw.source, draw.arrays[0] + next * 4)) < time) ++next;
        if (next == draw.count) throw std::runtime_error("Particle color phase exceeds its original curve.");
        const float end = floating(word(draw.source, draw.arrays[0] + next * 4));
        const float start = floating(word(draw.source, draw.arrays[0] + (next - 1) * 4));
        const float first = (end - time) / (end - start), second = (time - start) / (end - start);
        std::array<std::uint8_t, 4> result{};
        for (unsigned i = 0; i < 4; ++i) result[i] = static_cast<std::uint8_t>(int(
            second * draw.source->at(draw.arrays[array] + next * 4 + i) +
            first * draw.source->at(draw.arrays[array] + (next - 1) * 4 + i)));
        return result;
    }
}

particles::Billboards::Billboards(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> scale;
    std::string error;
    if (!dolresources::read(executable, 0x804dcac8, 4, scale, error)) throw std::runtime_error(error);
    childScale = floating((unsigned(scale[0]) << 24) | (unsigned(scale[1]) << 16) |
        (unsigned(scale[2]) << 8) | scale[3]);
    std::vector<std::uint8_t> geometry;
    if (!dolresources::read(executable, 0x803817c0, 48, geometry, error) ||
        !dolresources::read(executable, 0x804dcb08, 4, scale, error)) throw std::runtime_error(error);
    extendedPivotScale = floating((unsigned(scale[0]) << 24) | (unsigned(scale[1]) << 16) |
        (unsigned(scale[2]) << 8) | scale[3]);
    for (unsigned i = 0; i < 8; ++i) for (unsigned axis = 0; axis < 3; ++axis) {
        const unsigned offset = i * 6 + axis * 2;
        extendedCorners[i][axis] = float(static_cast<std::int16_t>((unsigned(geometry[offset]) << 8) | geometry[offset + 1]));
    }
    for (unsigned table = 0; table < 2; ++table) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, table ? glme01::cosineTable : glme01::sineTable, 4096 * 4, bytes, error))
            throw std::runtime_error(error);
        auto& values = table ? cosine : sine;
        for (unsigned i = 0; i < values.size(); ++i) values[i] = floating((unsigned(bytes[i * 4]) << 24) |
            (unsigned(bytes[i * 4 + 1]) << 16) | (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3]);
    }
}

std::vector<particles::Billboard> particles::Billboards::prepare(EmitterPool& pool, const roomlocation::Rooms& rooms,
    const roomtraversal::State& visibility, const std::array<float, 12>& view) const {
    std::vector<Billboard> result;
    auto& list = *pool.destination;
    for (auto* emitter = list.sentinel.next; emitter != &list.sentinel; emitter = emitter->next) {
        if (!(emitter->flags & 8)) continue;
        if (!(pool.flags & 1)) {
            const auto room = rooms.resolve(emitter->position);
            if (room == 0xffffffff && !emitter->nodes.count && !emitter->shortNodes.count) continue;
            const bool visible = room != 0xffffffff && (visibility.record(room, 0).flags & 3) == 1;
            if (visible) emitter->flags &= 0xffbf;
            else emitter->flags |= 0x40;
            if (!visible && !(emitter->flags & 0x80)) continue;
        }
        if (!emitter->nodes.count && !emitter->shortNodes.count) continue;
        if (!emitter->draw) throw std::runtime_error("Active particles have no drawing owner.");
        if (emitter->parameters.flags & 0x2000) appendChildren(*emitter, view, result);
        append(*emitter, view, result);
    }
    return result;
}

void particles::Billboards::appendChildren(Emitter& emitter, const std::array<float, 12>& view,
    std::vector<Billboard>& result) const {
    for (auto* node = emitter.shortNodes.sentinel.next; node != &emitter.shortNodes.sentinel; node = node->next) {
        Billboard item;
        item.color = node->fields20;
        item.color[3] = static_cast<std::uint8_t>(int(float(item.color[3]) *
            (float(int(node->fields18[0]) - int(node->fields18[1])) / float(node->fields18[0]))));
        item.texture = emitter.draw->alternateTexture;
        item.mode = 6;
        item.blend = emitter.parameters.fields206[1];
        item.depth = emitter.parameters.fields206[2];
        const float size = floating((unsigned(node->fields1A[2]) << 24) | (unsigned(node->fields1A[3]) << 16) |
            (unsigned(node->fields1A[4]) << 8) | node->fields1A[5]);
        const float radius = float(double(childScale) * double(size));
        for (unsigned corner = 0; corner < 4; ++corner) {
            const auto angle = static_cast<std::uint16_t>(0xe000 + corner * 0x4000) >> 4;
            for (unsigned axis = 0; axis < 3; ++axis) {
                const float basis = sine[angle] * view[axis] + cosine[angle] * view[4 + axis];
                item.positions[corner][axis] = float(double(node->fields0[axis] + node->fields0[axis + 3]) +
                    double(float(double(basis) * double(radius))));
            }
        }
        result.push_back(std::move(item));
    }
}

void particles::Billboards::append(Emitter& emitter, const std::array<float, 12>& view,
    std::vector<Billboard>& result) const {
    const auto& draw = *emitter.draw;
    for (auto* node = emitter.nodes.sentinel.next; node != &emitter.nodes.sentinel; node = node->next) {
        Billboard item;
        const float time = phase(draw, *node);
        item.color = color(draw, 1, time);
        if (draw.fields18[0] & 2) item.secondary = color(draw, 2, time);
        item.color[3] = static_cast<std::uint8_t>(int(float(item.color[3]) * node->fields38[2]));
        for (unsigned i = 0; i < 4; ++i) node->fields54[i] = item.color[i];
        item.texture = draw.texture;
        item.mode = draw.fields18[0];
        item.blend = emitter.parameters.fields206[1];
        item.depth = emitter.parameters.fields206[2];
        if (emitter.draw == &emitter.extendedDraw) {
            const auto cross = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
                return std::array<float, 3>{a[1] * b[2] - a[2] * b[1],
                    a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
            };
            const auto normalize = [](std::array<float, 3>& value) {
                const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
                if (length == 0) return false;
                for (auto& component : value) component /= length;
                return true;
            };
            const unsigned offset = draw.fields34[0] == 1 ? 0 : 3;
            std::array<float, 3> direction{node->fields0[offset], node->fields0[offset + 1], node->fields0[offset + 2]};
            const std::array<float, 3> previous{node->field48, node->fields4C[0], node->fields4C[1]};
            if (!normalize(direction)) { node->fields30[1] = node->fields30[0]; continue; }
            auto side = cross(previous, direction);
            if (!normalize(side)) { node->fields30[1] = node->fields30[0]; continue; }
            auto across = cross(direction, side);
            normalize(across);
            node->field48 = across[0];
            node->fields4C = {across[1], across[2]};
            const float size = node->fields38[1] * node->fields38[0];
            const float length = float(double(size) * double(floating(draw.fields2C[0])));
            const auto angle = node->fields44[0] >> 4;
            const float pivot = extendedPivotScale * floating(draw.fields2C[1]);
            item.cull = 0;
            for (unsigned plane = 0; plane < (draw.fields34[1] ? 2u : 1u); ++plane) {
                for (unsigned corner = 0; corner < 4; ++corner) {
                    const auto& point = extendedCorners[plane * 4 + corner];
                    const float x = cosine[angle] * point[0] - sine[angle] * point[2];
                    const float z = sine[angle] * point[0] + cosine[angle] * point[2];
                    const float y = point[1] + pivot;
                    for (unsigned axis = 0; axis < 3; ++axis) item.positions[corner][axis] =
                        node->fields0[axis] + floating(node->fields24[axis]) +
                        across[axis] * size * x + direction[axis] * length * y + side[axis] * size * z;
                }
                result.push_back(item);
            }
            continue;
        }
        const float size = (35.35534f * node->fields38[1]) * node->fields38[0];
        for (unsigned corner = 0; corner < 4; ++corner) {
            const auto angle = static_cast<std::uint16_t>(node->fields44[0] + 0xe000 + corner * 0x4000) >> 4;
            const float x = size * sine[angle], y = size * cosine[angle];
            for (unsigned axis = 0; axis < 3; ++axis) item.positions[corner][axis] =
                float(double(node->fields0[axis] + floating(node->fields24[axis])) +
                    double(x * view[axis] + y * view[4 + axis]));
        }
        result.push_back(std::move(item));
    }
}

std::vector<particles::Billboard> particles::prepareGlows(GlowPool& pool, Resource texture, std::uint32_t& randomState,
    const std::array<float, 12>& view) {
    std::vector<Billboard> result;
    constexpr std::array<float, 2> corners[] = {{-25, 25}, {25, 25}, {25, -25}, {-25, -25}};
    for (auto& glow : pool.entries()) {
        if (!glow.state) continue;
        if (!glow.position) throw std::runtime_error("Active glow has no original position owner.");
        randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
        const float flicker = 1.0f - 0.15f * (0x1p-32f * float(randomState));
        glow.color[3] = static_cast<std::uint8_t>(int(70.0 * double(glow.intensity * flicker)));
        Billboard item;
        item.color = glow.color;
        item.texture = texture;
        item.mode = 4;
        item.blend = 0x14;
        item.depth = 0x0b;
        item.cull = 0;
        for (unsigned corner = 0; corner < 4; ++corner)
            for (unsigned axis = 0; axis < 3; ++axis) item.positions[corner][axis] = (*glow.position)[axis] +
                (axis == 1 ? 14.0f * float(glow.height) : 0.0f) +
                glow.scale * (corners[corner][0] * view[axis] + corners[corner][1] * view[4 + axis]);
        result.push_back(std::move(item));
    }
    return result;
}

