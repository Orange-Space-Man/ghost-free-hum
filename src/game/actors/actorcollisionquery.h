#pragma once

#include <array>

namespace actorcollision {
    using Vector = std::array<float, 3>;

    struct Cylinder {
        float radius = 0, height = 0;
        Vector offset{};
    };

    struct Box {
        Vector size{}, offset{};
    };

    struct Shape {
        int kind = 0;
        const Cylinder* cylinder = nullptr;
        const Box* box = nullptr;
    };

    bool intersect(Vector& displacement, const Cylinder& first, const Vector& firstPosition,
        const Cylinder& second, const Vector& secondPosition);
    bool intersect(Vector& displacement, const Cylinder& first, const Vector& firstPosition,
        const Box& second, const Vector& secondPosition);
    bool intersect(Vector& displacement, const Box& first, const Vector& firstPosition,
        const Box& second, const Vector& secondPosition);
    bool intersect(Vector& displacement, const Shape& first, const Vector& firstPosition,
        const Shape& second, const Vector& secondPosition);
}
