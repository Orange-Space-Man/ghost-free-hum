#include "moneyscatterinitializer.h"

#include <cmath>
#include <stdexcept>

namespace {
    using moneyscatter::Vector;

    constexpr float randomDivisor = 4294967296.0f;
    constexpr float angleRange = 65535.0f;
    constexpr float directionSpread = 0.2f;
    constexpr Vector positionSpread{50, 0, 30};
    constexpr float sparklePeriod = 80;

    float unit(moneyscatter::InitializationServices& services) {
        return static_cast<float>(services.random()) / randomDivisor;
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    float length(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    void normalize(Vector& value) {
        const auto magnitude = length(value);
        if (magnitude >= 0.0001f) {
            const float factor = static_cast<float>(1.0 / magnitude);
            for (auto& component : value) component *= factor;
        } else value.fill(0);
    }

    actormodel::Matrix heading(Vector forward) {
        normalize(forward);
        Vector up{0, 1, 0}, difference{up[0] - forward[0], up[1] - forward[1], up[2] - forward[2]};
        if (length(difference) <= 0.0001f) up = {0, 0, 1};
        auto right = cross(up, forward);
        normalize(right);
        up = cross(forward, right);
        normalize(up);
        return {right[0], up[0], forward[0], 0, right[1], up[1], forward[1], 0,
            right[2], up[2], forward[2], 0};
    }

    Vector scatterDirection(const Vector& source, const actormodel::InitialPose& pose,
        moneyscatter::InitializationServices& services) {
        const auto yaw = pose.angle(source[2], source[0]);
        const float planarSquared = source[0] * source[0] + source[2] * source[2];
        const float planar = planarSquared > 0 ? static_cast<float>(double(planarSquared) /
            std::sqrt(double(planarSquared))) : planarSquared;
        const auto pitch = pose.angle(source[1], planar);
        const float yawJitter = 1.0f - 2.0f * unit(services);
        const float pitchJitter = 1.0f - 2.0f * unit(services);
        const auto offset = [](float jitter) {
            return static_cast<std::int32_t>(directionSpread * (angleRange * jitter));
        };
        const auto horizontal = pose.horizontalDirection(static_cast<std::uint16_t>(yaw + offset(yawJitter)));
        const auto vertical = pose.horizontalDirection(static_cast<std::uint16_t>(pitch + offset(pitchJitter)));
        return {vertical[0] * horizontal[0], vertical[2], vertical[0] * horizontal[2]};
    }
}

std::array<std::int16_t, 3> moneyscatter::directionAngles(const Vector& direction, const actormodel::InitialPose& pose) {
    return pose.angles(heading(direction));
}

void moneyscatter::initialize(const Request& request, Particle& particle, const actormodel::InitialPose& pose,
    InitializationServices& services, const std::optional<Vector>& retainedDirection) {
    if (request.lifetime >= 1000 && !retainedDirection)
        throw std::runtime_error("Placed money requires the original retained direction scratch value.");
    Vector direction;
    if (request.lifetime < 1000) {
        direction = scatterDirection(request.direction, pose, services);
        particle.forward = direction;
        particle.motion = 0;
        particle.spin = 360.0f * unit(services);
        particle.room = services.roomAt(particle.position, 0xffffffff);
        particle.currentRoom = particle.room;
        particle.placedMotion = false;
    } else {
        particle.forward = pose.horizontalDirection(static_cast<std::uint16_t>(angleRange * unit(services)));
        particle.motion = 3;
        particle.spin = unit(services) > 0.5f ? 270.0f : 90.0f;
        particle.placedMotion = true;
        direction = *retainedDirection;
    }
    for (unsigned i = 0; i < 3; ++i) particle.velocity[i] = direction[i] * request.speed;
    if (request.speed > 0) particle.angles = directionAngles(direction, pose);
    particle.acceleration = {};
    particle.position = request.position;
    if (request.positionMode) {
        Vector jitter;
        for (auto& value : jitter) value = 1.0f - 2.0f * unit(services);
        for (unsigned i = 0; i < 3; ++i)
            particle.position[i] = request.position[i] + positionSpread[i] * jitter[i];
    }
    particle.lifetime = request.lifetime;
    particle.scale = 1;
    particle.blinking = false;
    particle.blinkCounter = 0;
    particle.sparkleCounter = static_cast<std::uint8_t>(sparklePeriod * unit(services));
    particle.rotationPhase = 1;
    particle.up = {0, 0, 1};
    services.updateModel(particle);
}
