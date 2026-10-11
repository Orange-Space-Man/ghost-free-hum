#include "game/world/mapcollisionstate.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (half(bytes, offset) << 16) | half(bytes, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const auto bits = word(bytes, offset);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    std::uint32_t integer(float value) {
        if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0) return 0x80000000;
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    }

    std::array<float, 3> vector(const std::vector<std::uint8_t>& bytes, unsigned table, unsigned index) {
        const auto offset = std::size_t(word(bytes, table)) + std::size_t(index) * 12;
        return {number(bytes, offset), number(bytes, offset + 4), number(bytes, offset + 8)};
    }

    float dot(const std::array<float, 3>& first, const std::array<float, 3>& second) {
        return std::fma(first[0], second[0], first[1] * second[1]) + first[2] * second[2];
    }

    std::size_t polygonList(const std::vector<std::uint8_t>& bytes, const std::array<float, 3>& position, unsigned field) {
        std::array<float, 3> local;
        bool inside = true;
        for (unsigned i = 0; i < 3; ++i) {
            local[i] = position[i] - number(bytes, 12 + i * 4);
            if (local[i] < 0 || number(bytes, 24 + i * 4) <= local[i]) inside = false;
        }
        unsigned index = 0;
        if (inside) {
            const auto width = integer(number(bytes, 24) / number(bytes, 0)) + 1;
            const auto height = integer(number(bytes, 28) / number(bytes, 4)) + 1;
            const auto x = integer(local[0] / number(bytes, 0));
            const auto y = integer(local[1] / number(bytes, 4));
            const auto z = integer(local[2] / number(bytes, 8));
            const auto cell = word(bytes, 0x38) + x * 8 + width * y * 8 + height * (width * z) * 8;
            index = word(bytes, std::size_t(cell) + field);
        }
        return std::size_t(word(bytes, 0x30)) + std::size_t(index) * 2;
    }

    float polygonHeight(const std::vector<std::uint8_t>& bytes, const std::array<float, 3>& position, std::size_t polygon) {
        const auto vertex = vector(bytes, 0x24, half(bytes, polygon));
        const auto normal = vector(bytes, 0x28, half(bytes, polygon + 6));
        const float x = normal[0] * (position[0] - vertex[0]);
        const float z = normal[2] * (position[2] - vertex[2]);
        const float height = vertex[1] - (x + z) / normal[1];
        const std::array<float, 3> local{position[0] - vertex[0], height - vertex[1], position[2] - vertex[2]};
        if (!(dot(local, vector(bytes, 0x28, half(bytes, polygon + 8))) <= 0)) return -32000;
        if (!(dot(local, vector(bytes, 0x28, half(bytes, polygon + 10))) <= 0)) return -32000;
        const auto distance = dot(local, vector(bytes, 0x28, half(bytes, polygon + 12)));
        if (!(distance >= 0) || !(number(bytes, polygon + 16) >= distance)) return -32000;
        return height;
    }
}

float mapcollision::State::floorHeight(const std::array<float, 3>& position) const {
    auto list = polygonList(collision, position, 4);
    const auto* polygons = polygonInfo();
    if (!polygons) throw std::runtime_error("Original floor projection requires PolygonInfo.");
    polygons->searchItemInfo("ignore_pointer");
    float result = -32000;
    for (auto index = half(collision, list); index != 0xffff; index = half(collision, list += 2)) {
        const auto polygon = std::size_t(word(collision, 0x2c)) + std::size_t(index) * 24;
        if (!(half(collision, polygon + 20) & enabled)) continue;
        const auto height = polygonHeight(collision, position, polygon);
        if (height <= position[1] && result < height) result = height;
    }
    return result;
}

bool mapcollision::State::projectFloor(std::array<float, 3>& position) const {
    const auto height = floorHeight(position);
    if (height < -30000) return false;
    position[1] = height;
    return true;
}

std::array<std::uint16_t, 4> mapcollision::State::floorPolygons(const std::array<float, 3>& position, float distance) const {
    std::array<std::uint16_t, 4> result{0xffff, 0xffff, 0xffff, 0xffff};
    auto list = polygonList(collision, position, 4);
    unsigned count = 0;
    for (auto index = half(collision, list); index != 0xffff; index = half(collision, list += 2)) {
        const auto polygon = std::size_t(word(collision, 0x2c)) + std::size_t(index) * 24;
        if (!(half(collision, polygon + 20) & enabled)) continue;
        const float difference = position[1] - polygonHeight(collision, position, polygon);
        if (difference >= 0 && difference <= distance) {
            if (count < result.size()) result[count] = static_cast<std::uint16_t>(index);
            ++count;
        }
    }
    return result;
}

bool mapcollision::State::polygonNormal(int polygon, std::array<float, 3>& normal) const {
    if (polygon < 0) return false;
    const auto record = std::size_t(word(collision, 0x2c)) + std::size_t(polygon & 0xffff) * 24;
    normal = vector(collision, 0x28, half(collision, record + 6));
    return true;
}

bool mapcollision::State::ladderContact(std::uint16_t polygon, const std::array<float, 3>& position) const {
    if (polygon == 0xffff) return false;
    const auto* table = polygonInfo();
    if (!table) throw std::runtime_error("Original ladder query requires PolygonInfo.");
    std::uint8_t ladder = 0;
    if (!table->getValue(polygon, "hashigo", ladder) || !ladder) return false;
    const auto record = std::size_t(word(collision, 0x2c)) + std::size_t(polygon) * 24;
    const auto normal = vector(collision, 0x28, half(collision, record + 6));
    const auto vertex = vector(collision, 0x24, half(collision, record));
    return normal[1] == 1 && vertex[1] - position[1] < 50;
}

mapcollision::SphereContact mapcollision::State::sphereContact(const std::array<float, 3>& position, float radius) const {
    SphereContact result;
    auto center = position;
    auto list = polygonList(collision, center, 0);
    for (auto index = half(collision, list); index != 0xffff; index = half(collision, list += 2)) {
        const auto polygon = std::size_t(word(collision, 0x2c)) + std::size_t(index) * 24;
        if (!(half(collision, polygon + 20) & enabled)) continue;
        const auto vertex = vector(collision, 0x24, half(collision, polygon));
        const auto normal = vector(collision, 0x28, half(collision, polygon + 6));
        std::array<float, 3> local;
        for (unsigned i = 0; i < 3; ++i) local[i] = center[i] - vertex[i];
        const float distance = dot(local, normal);
        if (distance < 0 || radius < distance) continue;
        std::array<std::array<float, 3>, 3> edges;
        std::array<float, 3> distances;
        bool outside = false;
        for (unsigned i = 0; i < 3; ++i) {
            edges[i] = vector(collision, 0x28, half(collision, polygon + 8 + i * 2));
            distances[i] = dot(local, edges[i]);
            if (i == 2) distances[i] -= number(collision, polygon + 16);
            if (distances[i] > radius) outside = true;
        }
        if (outside) continue;
        auto direction = normal;
        float penetration = radius - distance;
        unsigned edge = 0;
        while (edge < 3 && distances[edge] <= 0) ++edge;
        if (edge < 3) {
            if (!(half(collision, polygon + 22) & (1u << edge))) continue;
            const unsigned starts[]{0, 2, 1}, ends[]{1, 0, 2};
            const auto start = vector(collision, 0x24, half(collision, polygon + starts[edge] * 2));
            const auto end = vector(collision, 0x24, half(collision, polygon + ends[edge] * 2));
            std::array<float, 3> segment, relative;
            for (unsigned i = 0; i < 3; ++i) {
                segment[i] = end[i] - start[i];
                relative[i] = center[i] - start[i];
            }
            float fraction = dot(relative, segment) / dot(segment, segment);
            for (unsigned i = 0; i < 3; ++i) {
                const float closest = fraction < 0 ? start[i] : fraction > 1 ? end[i] : start[i] + segment[i] * fraction;
                direction[i] = center[i] - closest;
            }
            const float length = std::sqrt(dot(direction, direction));
            if (length > radius) continue;
            const float lateral = dot(edges[edge], direction);
            const float reach = lateral < radius ? std::sqrt(radius * radius - lateral * lateral) : 0;
            if ((-0.00000011920928955078125f < reach && reach < 0.00000011920928955078125f) || reach < distance) continue;
            penetration = radius - length;
            for (auto& value : direction) value /= length;
        }
        for (unsigned i = 0; i < 3; ++i) {
            const float correction = direction[i] * penetration;
            result.correction[i] += correction;
            center[i] += correction;
        }
        if (normal[1] > 0.7f || normal[1] < -0.7f) {
            result.floor = int(index);
            result.floorNormal = normal;
        } else {
            result.wall = int(index);
            result.wallNormal = normal;
        }
    }
    return result;
}

mapcollision::SphereContact mapcollision::State::sweepSphere(const std::array<float, 3>& position, float radius,
    const std::array<float, 3>& displacement) const {
    float length = std::sqrt(dot(displacement, displacement));
    auto direction = displacement;
    if (length < 0.00000011920928955078125f || length >= 1000) {
        length = 0;
        direction.fill(0);
    } else {
        const float inverse = 1 / length;
        for (auto& value : direction) value *= inverse;
    }
    const float step = 0.9f * (radius > 1 ? radius : 1);
    for (float remaining = length - step; remaining > 0; remaining -= step) {
        auto sample = position;
        for (unsigned i = 0; i < 3; ++i) sample[i] -= direction[i] * remaining;
        auto contact = sphereContact(sample, radius);
        if (contact.floor == -1 && contact.wall == -1) continue;
        for (unsigned i = 0; i < 3; ++i) contact.correction[i] += -direction[i] * remaining;
        return contact;
    }
    return sphereContact(position, radius);
}
