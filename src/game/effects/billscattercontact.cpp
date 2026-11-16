#include "billscattercontact.h"

#include <cmath>

namespace {
    using billscatter::Vector;

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    Vector cross(const Vector& left, const Vector& right) {
        return {
            left[1] * right[2] - left[2] * right[1],
            left[2] * right[0] - left[0] * right[2],
            left[0] * right[1] - left[1] * right[0]
        };
    }

    void slide(Vector& velocity, const Vector& normal, float speed) {
        const float inverse = 1.0f / magnitude(velocity);
        Vector direction;
        for (unsigned i = 0; i < 3; ++i) direction[i] = -(velocity[i] * inverse);
        const auto tangent = cross(cross(direction, normal), normal);
        for (unsigned i = 0; i < 3; ++i) velocity[i] = tangent[i] * speed;
    }
}

void billscatter::collide(Particle& particle, float radius, float settlingSpeed, ContactServices& services) {
    const auto contact = services.sweep(particle.position, radius * particle.scale, particle.velocity);
    if (contact.floor >= 0 || contact.wall >= 0) {
        for (unsigned i = 0; i < 3; ++i) particle.position[i] += contact.correction[i];
    }
    if (contact.floor >= 0) {
        const float speed = magnitude(particle.velocity);
        if (speed <= settlingSpeed) particle.motion = 1;
        else slide(particle.velocity, contact.floorNormal, speed);
    }
    if (contact.wall >= 0) {
        const float speed = magnitude(particle.velocity);
        if (speed != 0) slide(particle.velocity, contact.wallNormal, speed);
    }
}
