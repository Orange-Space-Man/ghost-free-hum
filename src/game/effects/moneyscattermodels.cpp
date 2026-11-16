#include "moneyscattermodels.h"

#include <cmath>
#include <stdexcept>

namespace {
    using moneyscatter::Vector;

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    void normalize(Vector& value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length > 0) {
            const float inverse = 1.0f / length;
            for (auto& component : value) component *= inverse;
        }
    }
}

void moneyscatter::updateModel(Particle& particle) {
    if (!particle.model) throw std::runtime_error("Money particle requires its original J3D model instance.");
    auto right = cross(particle.forward, particle.up);
    normalize(right);
    particle.up = cross(right, particle.forward);
    normalize(particle.up);
    particle.model->scale = {particle.scale, particle.scale, particle.scale};
    constexpr float radiansPerDegree = 0.017453292f;
    const float radians = radiansPerDegree * particle.spin;
    const float sine = std::sin(radians), cosine = std::cos(radians);
    auto& matrix = particle.model->matrix;
    for (unsigned row = 0; row < 3; ++row) {
        matrix[row * 4] = right[row] * cosine - particle.up[row] * sine;
        matrix[row * 4 + 1] = particle.forward[row];
        matrix[row * 4 + 2] = right[row] * sine + particle.up[row] * cosine;
        matrix[row * 4 + 3] = particle.position[row];
    }
}

moneyscatter::Models::Models(Pool& pool, const std::shared_ptr<j3dmodel::ModelData>& coin, float retainedAuxiliarySpin) {
    if (!coin) throw std::runtime_error("Money model storage requires the original shared Coin model resource.");
    for (unsigned i = 0; i < 200; ++i)
        if (pool.particles.at(i).model) throw std::logic_error("Money particle model storage is already initialized.");
    for (unsigned i = 0; i < 200; ++i)
        pool.particles.at(i).model = std::make_unique<j3dmodel::ModelInstance>(coin, 0x10000);
    auxiliary.model = std::make_unique<j3dmodel::ModelInstance>(coin, 0x10000);
    auxiliary.forward = {0, 1, 0};
    auxiliary.up = {1, 0, 0};
    auxiliary.scale = 1;
    auxiliary.spin = retainedAuxiliarySpin;
}

moneyscatter::Models::Models(Pool& pool, const std::shared_ptr<j3dmodel::ModelData>& coin, RetainedState& state) :
    Models(pool, coin, state.auxiliarySpin) {
    retained = &state;
}

void moneyscatter::Models::updateAuxiliary(const Vector& cameraPosition, const Vector& cameraDirection) {
    auxiliary.spin += 3;
    if (auxiliary.spin > 360) auxiliary.spin -= 360;
    if (retained) retained->auxiliarySpin = auxiliary.spin;
    auto direction = cameraDirection;
    normalize(direction);
    for (unsigned i = 0; i < 3; ++i) auxiliary.position[i] = cameraPosition[i] - 10000.0f * direction[i];
    updateModel(auxiliary);
}
