#include "resources/dolresources.h"
#include "game/effects/particleforces.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    float floating(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::int32_t integer(float value) {
        return !std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0 ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
    }

    float dot(const Vector& first, const Vector& second) {
        return std::fma(first[0], second[0], first[1] * second[1]) + first[2] * second[2];
    }

    float square(const Vector& vector) {
        return std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1];
    }

    Vector normalized(const Vector& vector) {
        const float length = square(vector);
        const float estimate = 1.0f / std::sqrt(length);
        const float inverse = (3.0f - estimate * estimate * length) * (estimate * 0.5f);
        return {vector[0] * inverse, vector[1] * inverse, vector[2] * inverse};
    }

    std::array<float, 12> rotation(float angle, const Vector& vector) {
        const float sine = std::sin(angle), cosine = std::cos(angle), delta = 1.0f - cosine;
        const auto axis = normalized(vector);
        const float xy = axis[1] * (delta * axis[0]), xz = axis[2] * (delta * axis[0]);
        const float yz = axis[2] * (delta * axis[1]);
        return {cosine + delta * (axis[0] * axis[0]), xy - sine * axis[2], xz + sine * axis[1], 0,
            xy + sine * axis[2], cosine + delta * (axis[1] * axis[1]), yz - sine * axis[0], 0,
            xz - sine * axis[1], yz + sine * axis[0], cosine + delta * (axis[2] * axis[2]), 0};
    }
}

particles::Forces::Forces(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> pointers;
    std::string error;
    if (!dolresources::read(executable, 0x80381678, 24, pointers, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < noise.size(); ++i) {
        const unsigned address = (unsigned(pointers[i * 4]) << 24) | (unsigned(pointers[i * 4 + 1]) << 16) |
            (unsigned(pointers[i * 4 + 2]) << 8) | pointers[i * 4 + 3];
        std::vector<std::uint8_t> bytes;
        if (!dolresources::read(executable, address, 2048, bytes, error)) throw std::runtime_error(error);
        for (unsigned j = 0; j < noise[i].size(); ++j) noise[i][j] = (unsigned(bytes[j * 2]) << 8) | bytes[j * 2 + 1];
    }
}

void particles::Forces::apply(const Parameters& parameters, Node& node, std::uint32_t& randomState) const {
    auto& values = node.fields0;
    const auto flags = parameters.flags;
    if (flags & 0x10000) for (unsigned i = 0; i < 3; ++i) values[i + 6] += floating(parameters.fields168[i]);
    if (flags & 0x40000) {
        const Vector axis{floating(parameters.fields180[0]), floating(parameters.fields180[1]), floating(parameters.fields180[2])};
        const Vector position{values[0], values[1], values[2]};
        const float projection = dot(axis, position);
        Vector transverse;
        for (unsigned i = 0; i < 3; ++i) transverse[i] = position[i] - axis[i] * projection;
        const float distance = square(transverse), radius = floating(parameters.fields180[6]);
        const float weight = radius == 0 ? 1 : 1 + (floating(parameters.fields180[5]) * radius - distance) / (radius + distance);
        const float translation = floating(parameters.fields180[4]) * weight;
        for (auto& component : transverse) component *= translation;
        auto matrix = rotation(floating(parameters.fields180[3]) * weight, axis);
        for (unsigned i = 0; i < 3; ++i) {
            const auto offset = i * 4;
            matrix[offset + 3] = std::fma(matrix[offset + 2], transverse[2],
                std::fma(matrix[offset + 1], transverse[1], matrix[offset] * transverse[0])) + matrix[offset + 3];
            values[i] = std::fma(matrix[offset + 2], position[2], matrix[offset] * position[0]) +
                (matrix[offset + 3] + matrix[offset + 1] * position[1]);
        }
    }
    if (flags & 0x80000) {
        for (unsigned i = 0; i < 3; ++i) {
            values[i + 6] += floating(parameters.fields19C[3]) * (floating(parameters.fields19C[i]) - values[i]) - values[i + 3];
        }
    }
    if (flags & 0x100000) {
        for (unsigned i = 0; i < 3; ++i) values[i + 6] += floating(parameters.fields1AC[3]) * (floating(parameters.fields1AC[i]) - values[i]);
    }
    if (flags & 0x200000) {
        const unsigned shift = parameters.fields1C8[0], offset = parameters.fields1C8[1];
        if (shift > 25) throw std::runtime_error("Original particle noise shift exceeds the defined coordinate domain.");
        const auto& table = noise.at(parameters.fields1C8[2]);
        const std::int32_t bias = 16 << shift, period = 32 << shift;
        std::array<unsigned, 3> positive, negative;
        for (unsigned i = 0; i < 3; ++i) {
            const auto position = static_cast<std::uint32_t>(integer(values[i]));
            auto wrap = [&](std::uint32_t value) {
                auto result = static_cast<std::int32_t>(value) % period;
                if (result < 0) result += period;
                return static_cast<unsigned>(result) >> shift;
            };
            positive[i] = wrap(position + offset + bias);
            negative[i] = wrap(position - offset + bias);
        }
        const std::array<int, 3> gradient{
            int((table[positive[1] * 32 + positive[0]] >> 10) & 31) - int((table[negative[1] * 32 + negative[0]] >> 10) & 31),
            int((table[positive[2] * 32 + positive[1]] >> 5) & 31) - int((table[negative[2] * 32 + negative[1]] >> 5) & 31),
            int(table[positive[0] * 32 + positive[2]] & 31) - int(table[negative[0] * 32 + negative[2]] & 31)};
        for (unsigned i = 0; i < 3; ++i) values[i + 6] += floating(parameters.fields1BC[i]) * static_cast<float>(gradient[i]);
    }
    if (flags & 0x400000) {
        for (unsigned i = 0; i < 3; ++i) {
            randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
            const float amount = floating(parameters.field1CC);
            values[i + 6] += (0x1p-32f * static_cast<float>(randomState)) * (amount + amount) - amount;
        }
    }
    if (flags & 0x800000) {
        values[3] = values[4] = values[5] = 0;
        const Vector axis{floating(parameters.fields1D0[0]), floating(parameters.fields1D0[1]), floating(parameters.fields1D0[2])};
        const float projection = dot(axis, {values[0], values[1], values[2]});
        for (unsigned i = 0; i < 3; ++i) {
            const float transverse = axis[i] * projection - values[i];
            values[i + 6] = (values[i + 6] + floating(parameters.fields1D0[3]) * axis[i]) + floating(parameters.fields1D0[4]) * transverse;
        }
    }
    if (flags & 0x20000) for (unsigned i = 0; i < 3; ++i) values[i] += floating(parameters.fields174[i]);
}
