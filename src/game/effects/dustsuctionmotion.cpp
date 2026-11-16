#include "game/effects/dustsuctionmotion.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    float read(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    float magnitude(const Vector& value) {
        return std::sqrt(std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1]);
    }

    Vector normalized(const Vector& value) {
        const float square = std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1];
        const float estimate = 1 / std::sqrt(square);
        const float inverse = (3 - (estimate * estimate) * square) * (estimate * 0.5f);
        return {value[0] * inverse, value[1] * inverse, value[2] * inverse};
    }
}

runningdust::SuctionMotion::SuctionMotion(const std::vector<std::uint8_t>& executable) :
    nearDistance(read(executable, 0x804dc294)), minimumSize(read(executable, 0x804dc298)),
    nearDecay(read(executable, 0x804dc29c)), farDecay(read(executable, 0x804dc2a0)),
    transverseThreshold(read(executable, 0x804dc2a8)), maximumSpeed(read(executable, 0x804d591c)),
    one(read(executable, 0x804dc258)), rotationBase(read(executable, 0x804dc25c)),
    randomDivisor(read(executable, 0x804dc260)), outwardAcceleration(read(executable, 0x804d5920)),
    outwardVelocity(read(executable, 0x804d5924)), minimumOutwardVelocity(read(executable, 0x804d5928)) {
    for (unsigned i = 0; i < sine.size(); ++i) sine[i] = read(executable, glme01::sineTable + i * 4);
}

void runningdust::SuctionMotion::advance(float distance, float strength, const Vector& origin,
    const Vector& axis, const Vector& direction, const SuctionParameters& parameters,
    SuctionParticle& particle, const std::function<std::uint32_t()>& random) const {
    if (!random) throw std::runtime_error("Dust suction motion requires the original random stream.");
    decay(distance, particle);
    const float ratio = strength / distance;
    const float accelerationScale = ratio * parameters.accelerationScale;
    const float velocityScale = ratio * parameters.velocityScale;
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = particle.position[i] - origin[i];
    const float projection = std::fma(delta[0], axis[0], delta[1] * axis[1]) + delta[2] * axis[2];
    Vector transverse;
    for (unsigned i = 0; i < 3; ++i) transverse[i] = axis[i] * projection - delta[i];
    const float threshold = transverseThreshold * (one + float(random()) / randomDivisor);
    const auto force = magnitude(transverse) <= threshold ? direction : normalized(transverse);
    for (unsigned i = 0; i < 3; ++i) {
        particle.acceleration[i] = force[i] * accelerationScale + particle.acceleration[i];
        particle.velocity[i] = direction[i] * velocityScale;
        particle.velocity[i] = particle.acceleration[i] + particle.velocity[i];
    }
    finish(strength, parameters, particle);
}

void runningdust::SuctionMotion::decay(float distance, SuctionParticle& particle) const {
    if (distance <= nearDistance && particle.size > minimumSize) particle.baseSize *= nearDecay;
    else {
        particle.baseSize *= farDecay;
        if (particle.baseSize < minimumSize) particle.baseSize = minimumSize;
    }
}

void runningdust::SuctionMotion::outward(float distance, float strength, const Vector& direction,
    const SuctionParameters& parameters, SuctionParticle& particle,
    const std::function<Vector()>& randomDirection) const {
    if (!randomDirection) throw std::runtime_error("Outward dust motion requires the original direction random stream.");
    decay(distance, particle);
    const float ratio = distance > one ? strength / distance : strength;
    const float accelerationScale = ratio * parameters.accelerationScale;
    float velocityScale = outwardVelocity * (ratio * parameters.velocityScale);
    if (velocityScale < minimumOutwardVelocity) velocityScale = minimumOutwardVelocity;
    const auto random = randomDirection();
    const Vector cross{random[1] * direction[2] - random[2] * direction[1],
        random[2] * direction[0] - random[0] * direction[2], random[0] * direction[1] - random[1] * direction[0]};
    for (unsigned i = 0; i < 3; ++i) {
        particle.acceleration[i] = cross[i] * (accelerationScale * outwardAcceleration) + particle.acceleration[i];
        particle.velocity[i] = direction[i] * velocityScale;
        particle.velocity[i] = particle.acceleration[i] + particle.velocity[i];
    }
    finish(strength, parameters, particle);
}

void runningdust::SuctionMotion::finish(float strength, const SuctionParameters& parameters, SuctionParticle& particle) const {
    if (maximumSpeed < magnitude(particle.velocity)) {
        particle.velocity = normalized(particle.velocity);
        for (auto& component : particle.velocity) component *= maximumSpeed;
    }
    for (unsigned i = 0; i < 3; ++i) particle.position[i] += particle.velocity[i];
    particle.pulse = static_cast<std::uint16_t>(int(float(particle.pulse) + parameters.pulseStep));
    particle.size = particle.baseSize * ((one - parameters.pulseAmplitude) +
        parameters.pulseAmplitude * sine[particle.pulse >> 4]);
    particle.rotation += particle.rotationSpeed * (rotationBase + strength);
}
