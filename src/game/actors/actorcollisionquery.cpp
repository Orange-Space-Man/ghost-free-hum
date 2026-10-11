#include "game/actors/actorcollisionquery.h"

#include <cmath>
#include <stdexcept>

namespace {
    actorcollision::Vector add(const actorcollision::Vector& first, const actorcollision::Vector& second) {
        return {first[0] + second[0], first[1] + second[1], first[2] + second[2]};
    }

    float magnitude(float value) { return value <= 0 ? -value : value; }

    float direction(float value) {
        if (value <= 0) return value < 0 ? -1.0f : 0.0f;
        return 1;
    }

    float root(float value) {
        if (!(value > 0)) return value;
        const double input = value;
        double estimate = 1.0 / std::sqrt(input);
        for (unsigned i = 0; i < 3; ++i) {
            const double squared = estimate * estimate;
            estimate = (0.5 * estimate) * (3.0 - input * squared);
        }
        return static_cast<float>(input * estimate);
    }

    bool vertical(float firstBottom, float firstTop, float secondBottom, float secondTop, float& displacement) {
        if (firstTop >= secondTop && firstBottom <= secondTop) {
            if (firstBottom > secondBottom) displacement = secondTop - firstBottom;
            return true;
        }
        if (secondTop >= firstTop && secondBottom <= firstTop) {
            if (firstBottom < secondBottom) displacement = secondBottom - firstTop;
            return true;
        }
        return false;
    }

    template<class Type> const Type& required(const Type* value) {
        if (!value) throw std::runtime_error("Original collision shape has no parameter record.");
        return *value;
    }
}

bool actorcollision::intersect(Vector& output, const Cylinder& first, const Vector& firstPosition,
    const Cylinder& second, const Vector& secondPosition) {
    const float radius = first.radius + second.radius;
    const auto a = add(first.offset, firstPosition), b = add(second.offset, secondPosition);
    const float x = a[0] - b[0], z = a[2] - b[2];
    const float distanceSquared = x * x + z * z;
    if (distanceSquared >= radius * radius) return false;
    float y = first.height + second.height;
    if (!vertical(a[1], a[1] + first.height, b[1], b[1] + second.height, y)) return false;
    const float distance = root(distanceSquared);
    const float horizontal = radius - distance;
    if (horizontal > magnitude(y)) output = {0, y, 0};
    else if (distance == 0) output = {radius, 0, 0};
    else {
        const float scale = horizontal / distance;
        output = {x * scale, 0, z * scale};
    }
    return true;
}

bool actorcollision::intersect(Vector& output, const Cylinder& first, const Vector& firstPosition,
    const Box& second, const Vector& secondPosition) {
    const float halfX = 0.5f * second.size[0], halfZ = 0.5f * second.size[2];
    const float boundX = first.radius + halfX, boundZ = first.radius + halfZ;
    const auto a = add(first.offset, firstPosition), b = add(second.offset, secondPosition);
    float x = a[0] - b[0], z = a[2] - b[2];
    const float absX = magnitude(x), absZ = magnitude(z);
    if (absX >= boundX || absZ >= boundZ) return false;
    unsigned axis;
    float horizontal, distance = 0;
    if (halfX < absX) {
        if (halfZ < absZ) {
            x = a[0] - (b[0] + halfX * direction(x));
            z = a[2] - (b[2] + halfZ * direction(z));
            const float squared = x * x + z * z;
            if (squared >= first.radius * first.radius) return false;
            axis = 3;
            distance = root(squared);
            horizontal = first.radius - distance;
        } else {
            axis = 1;
            horizontal = boundX * direction(x) - x;
        }
    } else {
        axis = 2;
        horizontal = boundZ * direction(z) - z;
    }
    const float halfY = 0.5f * second.size[1];
    float y = first.height + second.size[1];
    if (!vertical(a[1], a[1] + first.height, b[1] - halfY, b[1] + halfY, y)) return false;
    if (magnitude(horizontal) > magnitude(y)) output = {0, y, 0};
    else if (axis == 1) output = {horizontal, 0, 0};
    else if (axis == 2) output = {0, 0, horizontal};
    else if (distance == 0) output = {horizontal * direction(x), 0, 0};
    else {
        const float scale = horizontal / distance;
        output = {x * scale, 0, z * scale};
    }
    return true;
}

bool actorcollision::intersect(Vector& output, const Box& first, const Vector& firstPosition,
    const Box& second, const Vector& secondPosition) {
    const auto a = add(first.offset, firstPosition), b = add(second.offset, secondPosition);
    Vector difference{}, depth{};
    for (unsigned i = 0; i < 3; ++i) {
        const float bound = 0.5f * first.size[i] + 0.5f * second.size[i];
        difference[i] = a[i] - b[i];
        const float absolute = magnitude(difference[i]);
        if (absolute >= bound) return false;
        depth[i] = bound - absolute;
    }
    unsigned axis = 0;
    if (depth[1] <= depth[0] || depth[2] <= depth[0]) axis = depth[2] <= depth[1] ? 2 : 1;
    output = {};
    output[axis] = depth[axis] * direction(difference[axis]);
    return true;
}

bool actorcollision::intersect(Vector& output, const Shape& first, const Vector& firstPosition,
    const Shape& second, const Vector& secondPosition) {
    if (first.kind == 1) {
        if (second.kind == 1) return intersect(output, required(first.cylinder), firstPosition, required(second.cylinder), secondPosition);
        if (second.kind == 2) return intersect(output, required(first.cylinder), firstPosition, required(second.box), secondPosition);
    } else if (first.kind == 2) {
        if (second.kind == 2) return intersect(output, required(first.box), firstPosition, required(second.box), secondPosition);
        if (second.kind == 1) {
            const bool result = intersect(output, required(second.cylinder), secondPosition, required(first.box), firstPosition);
            for (auto& value : output) value = -value;
            return result;
        }
    }
    return false;
}
