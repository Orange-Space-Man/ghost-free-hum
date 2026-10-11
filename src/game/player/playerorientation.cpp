#include "game/player/playerorientation.h"

#include <cmath>

namespace {
    float squareRoot(float value) {
        if (!(0 < value)) return value;
        const double input = value;
        double estimate = 1.0 / std::sqrt(input);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        return static_cast<float>(input * 0.5 * estimate * (3.0 - input * estimate * estimate));
    }
}

playerorientation::Quaternion playerorientation::fromMatrix(const actormodel::Matrix& m) {
    Quaternion result;
    float trace = (m[10] + m[0]) + m[5];
    if (trace <= 0) {
        unsigned first = m[0] < m[5] ? 1 : 0;
        if (m[first * 5] < m[10]) first = 2;
        const unsigned second = (first + 1) % 3, third = (first + 2) % 3;
        const float root = squareRoot(1.0f + (m[first * 5] - (m[second * 5] + m[third * 5])));
        const float factor = 0.5f / root;
        result[first] = 0.5f * root;
        result[3] = factor * (m[second * 4 + third] - m[third * 4 + second]);
        result[second] = factor * (m[first * 4 + second] + m[second * 4 + first]);
        result[third] = factor * (m[first * 4 + third] + m[third * 4 + first]);
    } else {
        const float root = squareRoot(1.0f + trace);
        const float factor = 0.5f / root;
        result[3] = 0.5f * root;
        result[0] = factor * (m[6] - m[9]);
        result[1] = factor * (m[8] - m[2]);
        result[2] = factor * (m[1] - m[4]);
    }
    return result;
}

actormodel::Matrix playerorientation::toMatrix(const Quaternion& q) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float norm = ((w * w + z * z) + x * x) + y * y;
    const float factor = norm == 0 ? 1 : 2.0f / norm;
    const float zFactor = z * factor, yFactor = y * factor;
    const float xSquare = (x * x) * factor;
    const float wx = (w * x) * factor;
    return {1.0f - (y * yFactor + z * zFactor), x * yFactor + w * zFactor, x * zFactor - w * yFactor, 0,
        x * yFactor - w * zFactor, 1.0f - (z * zFactor + xSquare), y * zFactor + wx, 0,
        x * zFactor + w * yFactor, y * zFactor - wx, 1.0f - (xSquare + y * yFactor), 0};
}

actormodel::Matrix playerorientation::blend(double weight, const actormodel::Matrix& first, const actormodel::Matrix& second) {
    const auto a = fromMatrix(first), b = fromMatrix(second);
    const float inverse = static_cast<float>(1.0 - weight);
    const float product = a[3] * b[3] + (a[2] * b[2] + (a[0] * b[0] + a[1] * b[1]));
    const double signedWeight = product >= 0 ? weight : -weight;
    Quaternion quaternion;
    for (unsigned i = 0; i < 4; ++i) quaternion[i] = static_cast<float>(double(inverse) * a[i] + signedWeight * b[i]);
    auto result = toMatrix(quaternion);
    for (unsigned i : {3u, 7u, 11u}) result[i] = first[i] + (second[i] - first[i]) * static_cast<float>(weight);
    return result;
}

std::array<float, 3> playerorientation::direction(const Quaternion& quaternion) {
    const auto matrix = toMatrix(quaternion);
    std::array<float, 3> result{matrix[2], matrix[6], matrix[10]};
    const float magnitude = std::sqrt((result[0] * result[0] + result[1] * result[1]) + result[2] * result[2]);
    if (magnitude >= 0.0001f) {
        const float factor = 1.0f / magnitude;
        for (auto& value : result) value *= factor;
    } else result.fill(0);
    return result;
}
