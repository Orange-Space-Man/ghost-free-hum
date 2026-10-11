#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/effects/particleemission.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    float floating(std::uint32_t bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::int32_t integer(float value) {
        return !std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0 ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
    }

    std::uint32_t random(std::uint32_t& state) {
        state = state * 0x5eedf715u + 0x1b0cb173u;
        return state;
    }

    float unit(std::uint32_t& state) { return 0x1p-32f * static_cast<float>(random(state)); }

    Vector scaled(const Vector& vector, float scale) { return {vector[0] * scale, vector[1] * scale, vector[2] * scale}; }

    Vector added(const Vector& first, const Vector& second) {
        return {first[0] + second[0], first[1] + second[1], first[2] + second[2]};
    }

    float square(const Vector& vector) {
        return std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1];
    }

    Vector normalized(const Vector& vector) {
        const float length = square(vector), estimate = 1.0f / std::sqrt(length);
        return scaled(vector, (3.0f - (estimate * estimate) * length) * (estimate * 0.5f));
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    float parameter(const particles::Emitter& emitter, unsigned index) {
        const auto& parameters = emitter.parameters;
        const unsigned step = 0x40u << (index * 2), mask = step * 3;
        if (!(parameters.flags & mask)) {
            return floating(index == 0 ? parameters.fieldF4 : index == 1 ? parameters.field104 : parameters.field108);
        }
        const auto& curve = parameters.curves[index];
        const float frame = static_cast<float>(emitter.fieldBC);
        const float end = static_cast<float>(static_cast<std::int16_t>(parameters.field204));
        unsigned key = 1;
        if (parameters.flags & step) {
            while (!(frame <= end * floating(curve.value(false, key)))) ++key;
            return floating(curve.value(true, key - 1));
        }
        const float time = frame / end;
        if (index == 2) while (!(time <= floating(curve.value(false, key)))) ++key;
        else while (floating(curve.value(false, key)) < time) ++key;
        const float first = floating(curve.value(false, key - 1)), second = floating(curve.value(false, key));
        return (floating(curve.value(true, key)) * (time - first) +
            floating(curve.value(true, key - 1)) * (second - time)) / (second - first);
    }
}

particles::Emission::Emission(const std::vector<std::uint8_t>& executable) {
    std::string error;
    for (unsigned table = 0; table < 2; ++table) {
        std::vector<std::uint8_t> bytes;
        if (!dolresources::read(executable, table ? glme01::cosineTable : glme01::sineTable, 4096 * 4, bytes, error)) {
            throw std::runtime_error(error);
        }
        auto& values = table ? cosine : sine;
        for (unsigned i = 0; i < values.size(); ++i) {
            values[i] = floating((unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
                (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3]);
        }
    }
}

std::array<float, 3> particles::Emission::randomVector(std::uint32_t& randomState) {
    angles[0] = random(randomState) & 0xfff;
    angles[1] = random(randomState) & 0xfff;
    angleSine = sine[angles[0]];
    return {angleSine * cosine[angles[1]], angleSine * sine[angles[1]], cosine[angles[0]]};
}

void particles::Emission::emit(Emitter& emitter, std::uint32_t& randomState) {
    const auto& parameters = emitter.parameters;
    const float rate = parameter(emitter, 0), variation = rate * floating(parameters.fieldF8);
    emitter.fieldB8 += rate + (unit(randomState) * (variation + variation) - variation);
    const auto count = integer(emitter.fieldB8);
    emitter.fieldB8 -= static_cast<float>(count);
    Vector direction = emitter.field220;
    if (direction == Vector{}) {
        for (unsigned i = 0; i < 3; ++i) direction[i] = floating(parameters.fieldsD0[i + 3]);
    }
    direction = square(direction) == 0 ? randomVector(randomState) : normalized(direction);
    Vector extent = emitter.field22C;
    if (extent == Vector{}) for (unsigned i = 0; i < 3; ++i) extent[i] = floating(parameters.fieldsD0[i + 6]);
    const float speed = parameter(emitter, 2), radius = parameter(emitter, 1);
    float inner = floating(parameters.field100) * radius;
    const Vector origin{floating(parameters.fieldsD0[0]), floating(parameters.fieldsD0[1]), floating(parameters.fieldsD0[2])};
    auto velocity = [&](const Vector& axis) {
        const float variation = speed * floating(parameters.fields10C[0]);
        return scaled(axis, speed + (unit(randomState) * (variation + variation) - variation));
    };
    for (std::int32_t i = 0; i < count; ++i) {
        if (!emitter.nodePool) throw std::runtime_error("Original particle emission pool is unavailable.");
        if (!emitter.nodePool->count) return;
        Vector position, motion;
        const unsigned shape = parameters.flags & 3, mode = parameters.flags & 0xc;
        if (shape == 1) {
            if (mode == 4) {
                const auto axis = randomVector(randomState);
                position = added(origin, scaled(axis, inner + (radius - inner) * unit(randomState)));
                motion = velocity(axis);
            } else if (mode == 8) {
                position = origin;
                auto offset = cross(randomVector(randomState), direction);
                offset = scaled(offset, floating(parameters.fieldFC) * unit(randomState));
                motion = velocity(normalized(added(direction, offset)));
            } else {
                auto axis = cross(randomVector(randomState), direction);
                if (square(axis) == 0) continue;
                axis = normalized(axis);
                position = added(origin, scaled(axis, inner + (radius - inner) * unit(randomState)));
                motion = velocity(axis);
            }
        } else if (shape == 2) {
            for (unsigned axis = 0; axis < 3; ++axis) position[axis] = origin[axis] + extent[axis] * (2.0f * unit(randomState) - 1.0f);
            motion = velocity(direction);
        } else {
            inner = floating(parameters.field100);
            auto axis = cross(randomVector(randomState), direction);
            if (square(axis) == 0) continue;
            axis = normalized(axis);
            const auto radial = scaled(axis, extent[0]);
            const auto axial = scaled(direction, extent[1] * (2.0f * unit(randomState) - 1.0f));
            position = added(origin, scaled(added(radial, axial), 1.0f + (inner - 1.0f) * unit(randomState)));
            motion = velocity(mode == 8 ? direction : axis);
        }
        auto* node = emitter.nodePool->take();
        if (!node) throw std::runtime_error("Original particle free list is inconsistent.");
        emitter.nodes.append(*node);
        for (unsigned axis = 0; axis < 3; ++axis) {
            std::memcpy(&node->fields24[axis], &emitter.position[axis], sizeof(float));
            node->fields0[axis] = position[axis];
            node->fields0[axis + 3] = motion[axis];
        }
        const float sizeVariation = floating(parameters.fields10C[9]);
        node->fields38[1] = floating(parameters.fields10C[8]) * (1.0f + (unit(randomState) * (sizeVariation + sizeVariation) - sizeVariation));
        node->fields44[1] = static_cast<std::uint16_t>(integer(static_cast<float>(static_cast<std::int16_t>(parameters.fields140[0])) +
            static_cast<float>(static_cast<std::int16_t>(parameters.fields140[1])) * (2.0f * unit(randomState) - 1.0f)));
        node->fields44[0] = static_cast<std::uint16_t>(integer(static_cast<float>(static_cast<std::int16_t>(parameters.fields140[2])) *
            (2.0f * unit(randomState) - 1.0f)));
        node->fields30[0] = static_cast<std::uint16_t>(integer(static_cast<float>(static_cast<std::int16_t>(parameters.field14C)) *
            (1.0f - floating(parameters.field148) * unit(randomState))));
        node->field34 = static_cast<std::uint8_t>(integer(static_cast<float>(parameters.field14E) * unit(randomState)));
    }
}
