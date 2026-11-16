#include "billscattermotion.h"

#include <algorithm>
#include <cmath>

namespace {
    using billscatter::Vector;

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }
    void normalize(Vector& value) {
        const float inverse = 1.0f / magnitude(value);
        for (auto& component : value) component *= inverse;
    }
    void limit(Vector& value, float maximum) {
        if (magnitude(value) > maximum) {
            normalize(value);
            for (auto& component : value) component *= maximum;
        }
    }
    Vector cross(const Vector& a, const Vector& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
}

billscatter::Vector billscatter::flutterDirection(std::uint32_t& randomState, const actormodel::InitialPose& pose) {
    randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
    const auto first = pose.horizontalDirection(static_cast<std::uint16_t>((randomState % 0x800) << 4));
    randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
    const auto second = pose.horizontalDirection(static_cast<std::uint16_t>((randomState % 0x800) << 4));
    return {first[0] * second[2], first[0] * second[0], first[2]};
}

void billscatter::move(Particle& particle, const Request& request, bool ordinaryMotion, float flutterStrength,
    std::uint8_t flutterThreshold, std::uint32_t& particleRandom, std::uint32_t& gameplayRandom,
    const actormodel::InitialPose& pose, ContactServices& services, const MotionParameters& parameters) {
    if (ordinaryMotion) particle.scale = std::min(particle.scale * 1.15f, parameters.maximumScale);
    if (particle.motion != 0) return;
    Vector force = particle.acceleration;
    if (ordinaryMotion) {
        if (request.delay) {
            Vector direction;
            for (unsigned i = 0; i < 3; ++i) direction[i] = particle.position[i] - request.position[i];
            if (magnitude(direction) > 0) normalize(direction);
            else direction = {0, 1, 0};
            for (unsigned i = 0; i < 3; ++i) particle.velocity[i] += direction[i] * request.strength;
        }
        if (magnitude(particle.velocity) > 0) {
            particle.forward = particle.velocity;
            normalize(particle.forward);
        }
        const float alignment = std::abs(-particle.forward[1]);
        const float verticalDrag = parameters.drag * (0.6f + (1.0f - 0.6f) * alignment);
        const float horizontalDrag = parameters.drag * (0.6f + (1.0f - 0.6f) * (1.0f - alignment));
        particle.velocity[1] *= verticalDrag;
        particle.velocity[0] *= horizontalDrag;
        particle.velocity[2] *= horizontalDrag;
        for (auto& component : particle.acceleration) component *= parameters.accelerationDamping;
        if (flutterThreshold < particle.flutterCounter) {
            const auto direction = flutterDirection(particleRandom, pose);
            const auto sideways = cross(direction, particle.forward);
            gameplayRandom = gameplayRandom * 0x19660du + 0x3c6ef35fu;
            const float random = static_cast<float>(gameplayRandom) / 4294967296.0f;
            const float strength = (flutterStrength + parameters.flutterAlignment * alignment) *
                (1.0f - parameters.flutterVariation * parameters.flutterVariation * random);
            for (unsigned i = 0; i < 3; ++i) particle.acceleration[i] += sideways[i] * strength;
            particle.flutterCounter = 0;
        } else ++particle.flutterCounter;
        force = particle.acceleration;
        force[1] -= parameters.gravity;
        limit(particle.acceleration, 9);
    }
    for (unsigned i = 0; i < 3; ++i) particle.velocity[i] += force[i];
    limit(particle.velocity, 33);
    for (unsigned i = 0; i < 3; ++i) particle.position[i] += particle.velocity[i];
    collide(particle, 20, parameters.settlingSpeed, services);
}
