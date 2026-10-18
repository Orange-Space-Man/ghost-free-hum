#include "billscattermodels.h"

#include <cmath>
#include <stdexcept>

namespace {
    using billscatter::Vector;

    Vector cross(const Vector& a, const Vector& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    void normalize(Vector& value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length > 0) {
            const float inverse = 1.0f / length;
            for (auto& component : value) component *= inverse;
        }
    }
}

void billscatter::updateModel(Particle& particle) {
    if (!particle.model) throw std::runtime_error("Bill particle requires its original J3D model instance.");
    auto side = cross(particle.up, particle.forward);
    normalize(side);
    particle.up = cross(particle.forward, side);
    normalize(particle.up);
    particle.model->scale = {particle.scale, particle.scale, particle.scale};
    constexpr float radiansPerDegree = 0.017453292f;
    const float radians = radiansPerDegree * particle.spin;
    const float sine = std::sin(radians), cosine = std::cos(radians);
    auto& matrix = particle.model->matrix;
    for (unsigned row = 0; row < 3; ++row) {
        matrix[row * 4] = particle.forward[row];
        matrix[row * 4 + 1] = side[row] * cosine + particle.up[row] * sine;
        matrix[row * 4 + 2] = -side[row] * sine + particle.up[row] * cosine;
        matrix[row * 4 + 3] = particle.position[row];
    }
}
