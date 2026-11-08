#include "game/effects/particlebillboards.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cstring>
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
            const bool visible = (visibility.record(room, 0).flags & 3) == 1;
            if (visible) emitter->flags &= 0xffbf;
            else emitter->flags |= 0x40;
            if (!visible && !(emitter->flags & 0x80)) continue;
        }
        if (!emitter->nodes.count && !emitter->shortNodes.count) continue;
        if (emitter->draw != &emitter->ordinaryDraw || emitter->shortNodes.count)
            throw std::runtime_error("Particle draw requires the extended or child geometry path.");
        append(*emitter, view, result);
    }
    return result;
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
