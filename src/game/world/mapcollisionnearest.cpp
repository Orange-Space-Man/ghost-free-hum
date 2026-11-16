#include "game/world/mapcollisionstate.h"
#include <cmath>
#include <cstring>

namespace {
    using Vector = std::array<float, 3>;
    unsigned half(const std::vector<std::uint8_t>& b, std::size_t p) {
        return (unsigned(b.at(p)) << 8) | b.at(p + 1);
    }
    unsigned word(const std::vector<std::uint8_t>& b, std::size_t p) {
        return (half(b, p) << 16) | half(b, p + 2);
    }
    float number(const std::vector<std::uint8_t>& b, std::size_t p) {
        const auto bits = word(b, p);
        float result; std::memcpy(&result, &bits, 4); return result;
    }
    Vector vector(const std::vector<std::uint8_t>& b, unsigned table, unsigned index) {
        const auto p = std::size_t(word(b, table)) + index * 12;
        return {number(b, p), number(b, p + 4), number(b, p + 8)};
    }
    Vector difference(const Vector& a, const Vector& b) {
        return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    }
    float dot(const Vector& a, const Vector& b) {
        return (a[2] * b[2] + a[0] * b[0]) + a[1] * b[1];
    }
    float length(const Vector& value) {
        const double squared = dot(value, value);
        return squared > 0 ? static_cast<float>(std::sqrt(squared)) : static_cast<float>(squared);
    }
    float segmentDistance(const Vector& point, const Vector& first, const Vector& second) {
        const auto edge = difference(second, first), offset = difference(point, first);
        const float fraction = dot(edge, offset) / dot(edge, edge);
        Vector projected;
        for (unsigned i = 0; i < 3; ++i) projected[i] = first[i] + edge[i] * fraction;
        const float distance = length(difference(projected, point));
        if (!(fraction >= 0)) return length(difference(point, first));
        if (fraction <= 1) return distance;
        return length(difference(point, second));
    }
    float triangleDistance(const std::vector<std::uint8_t>& bytes, const Vector& point, unsigned index) {
        const auto record = std::size_t(word(bytes, 0x2c)) + index * 24;
        const auto first = vector(bytes, 0x24, half(bytes, record));
        const auto second = vector(bytes, 0x24, half(bytes, record + 2));
        const auto third = vector(bytes, 0x24, half(bytes, record + 4));
        const auto local = difference(point, first);
        auto projection = [&](unsigned offset) { return dot(local, vector(bytes, 0x28, half(bytes, record + offset))); };
        if (!(projection(8) <= 0)) return segmentDistance(point, third, first);
        if (!(projection(10) <= 0)) return segmentDistance(point, first, second);
        if (!(projection(12) <= number(bytes, record + 16))) return segmentDistance(point, second, third);
        const float distance = projection(6);
        return distance > 0 ? distance : -distance;
    }
}

mapcollision::NearestPolygon mapcollision::State::nearestPolygon(const Vector& position, float radius) const {
    NearestPolygon result;
    std::array<int, 3> low{}, high{}, dimensions{};
    for (unsigned i = 0; i < 3; ++i) {
        float minimum = (position[i] - radius) - number(collision, 12 + i * 4);
        float maximum = (position[i] + radius) - number(collision, 12 + i * 4);
        const float extent = number(collision, 24 + i * 4), cell = number(collision, i * 4);
        if (minimum <= -0.00000011920928955078125f) minimum = 0;
        if (extent <= maximum) maximum = extent;
        low[i] = static_cast<std::int16_t>(static_cast<int>(minimum / cell));
        high[i] = static_cast<std::int16_t>(static_cast<int>(maximum / cell));
        dimensions[i] = static_cast<int>(extent / cell) + 1;
    }
    for (int z = low[2]; z <= high[2]; ++z) {
        for (int y = low[1]; y <= high[1]; ++y) {
            for (int x = low[0]; x <= high[0]; ++x) {
                const auto cell = std::size_t(word(collision, 0x38)) +
                    static_cast<std::size_t>((z * dimensions[1] + y) * dimensions[0] + x) * 8;
                auto list = std::size_t(word(collision, 0x30)) + word(collision, cell) * 2;
                for (auto index = half(collision, list); index != 0xffff; index = half(collision, list += 2)) {
                    const auto distance = triangleDistance(collision, position, index);
                    if (distance <= radius && (result.polygon == 0xffff || distance < result.distance)) {
                        result.polygon = static_cast<std::uint16_t>(index);
                        result.distance = distance;
                    }
                }
            }
        }
    }
    return result;
}
