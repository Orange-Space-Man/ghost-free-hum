#include "game/player/playervacuummodel.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    float dot(const Vector& first, const Vector& second) {
        return std::fma(first[0], second[0], first[1] * second[1]) + first[2] * second[2];
    }

    float square(const Vector& vector) {
        return std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1];
    }

    Vector normalized(const Vector& vector) {
        const float length = std::sqrt(square(vector));
        if (!(length >= 0.0001f)) return {};
        const float inverse = 1.0f / length;
        return {vector[0] * inverse, vector[1] * inverse, vector[2] * inverse};
    }
}

void playermodels::VacuumModel::updateSegments() {
    for (unsigned i = 0; i < count - 1; ++i) {
        auto& segment = segments[i];
        segment.field18 = field9C * static_cast<float>(i);
        segment.field1C = field9C * static_cast<float>(i + 1);
        const auto& first = points[i].position;
        const auto& second = points[i + 1].position;
        const auto axis = normalized({second[0] - first[0], second[1] - first[1], second[2] - first[2]});
        segment.fieldC = axis[0];
        segment.field10 = axis[1];
        segment.field14 = axis[2];
        for (unsigned component = 0; component < 3; ++component) {
            segment.position[component] = second[component] - segment.field1C * axis[component];
        }
    }
}

bool playermodels::VacuumModel::intersects(double radius, std::array<float, 3>& direction,
    const std::array<float, 3>& position) const {
    const auto& origin = points[0].position;
    const Vector delta{origin[0] - position[0], origin[1] - position[1], origin[2] - position[2]};
    const float distance = square(delta);
    if (parameters.field20 * parameters.field20 <= distance) return false;
    if (static_cast<float>(radius * radius) > distance || parameters.field4C) {
        direction = normalized(delta);
        return true;
    }
    for (unsigned i = 0; i < count - 1; ++i) {
        Vector result;
        if (intersectsSegment(radius, result, position, i)) {
            direction = result;
            return true;
        }
    }
    return false;
}

bool playermodels::VacuumModel::intersectsSegment(double radius, std::array<float, 3>& direction,
    const std::array<float, 3>& position, unsigned index) const {
    if (index >= count - 1) throw std::out_of_range("Original vacuum segment index is invalid.");
    const auto& segment = segments[index];
    const Vector delta{position[0] - segment.position[0], position[1] - segment.position[1], position[2] - segment.position[2]};
    const Vector axis{segment.fieldC, segment.field10, segment.field14};
    const float projection = dot(delta, axis);
    if (!(projection >= segment.field18) || projection >= segment.field1C) return false;
    const auto unit = normalized(delta);
    float threshold = fieldB4;
    if (radius != 0) {
        const float width = static_cast<float>(radius / fieldB4) + static_cast<float>(index) * fieldB8;
        float length = segment.field1C * segment.field1C + width * width;
        if (!(length <= 0)) length = std::sqrt(length);
        threshold = segment.field1C / length;
    }
    if (!(dot(unit, axis) >= threshold)) return false;
    direction = {-unit[0], -unit[1], -unit[2]};
    return true;
}
