#include "moneyscatterupdate.h"

#include <algorithm>
#include <cmath>

namespace {
    using moneyscatter::Vector;
    constexpr int airborne = 0, rolling = 1, settling = 2, resting = 3;
    constexpr float baseSpin = 16, speedSpin = 1.2f, settleStep = 1.35f;
    constexpr unsigned sparklePeriod = 80;

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    void normalize(Vector& value) {
        const float factor = 1.0f / magnitude(value);
        for (auto& component : value) component *= factor;
    }

    void accelerateSuction(moneyscatter::Particle& particle, moneyscatter::UpdateServices& services) {
        constexpr float speedMultiplier = 8, maximumSpeed = 35, scaleDistance = 100;
        for (auto& component : particle.velocity) component *= speedMultiplier;
        if (magnitude(particle.velocity) > maximumSpeed) {
            normalize(particle.velocity);
            for (auto& component : particle.velocity) component *= maximumSpeed;
        }
        const auto origin = services.suctionPosition();
        const Vector delta{particle.position[0] - origin[0], particle.position[1] - origin[1], particle.position[2] - origin[2]};
        const float distance = magnitude(delta);
        const float minimum = services.suctionMinimumScale();
        particle.scale = distance > scaleDistance ? 1.0f : minimum + ((1.0f - minimum) * distance) / scaleDistance;
        particle.motion = airborne;
        particle.rotationPhase = particle.angularVelocity = 0;
    }
}

void moneyscatter::approachAngles(Particle& particle, const std::array<std::int16_t, 3>& target,
    bool ordinaryMotion, const actormodel::InitialPose& pose) {
    const unsigned step = ordinaryMotion ? 1000 : 5000;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned current = static_cast<std::uint16_t>(particle.angles[i]);
        const unsigned difference = (static_cast<std::uint16_t>(target[i]) - current) & 0xffff;
        const unsigned value = difference < 0x8000 ? current + std::min(difference, step) :
            current - std::min(0xffff - difference, step);
        particle.angles[i] = static_cast<std::int16_t>(value);
    }
    const auto matrix = pose.rotation(particle.angles);
    particle.forward = {matrix[2], matrix[6], matrix[10]};
    const float length = magnitude(particle.forward);
    if (length >= 0.0001f) {
        const float factor = static_cast<float>(1.0 / length);
        for (auto& component : particle.forward) component *= factor;
    } else particle.forward = {};
}

void moneyscatter::advanceOrientation(Particle& particle, bool ordinaryMotion, const actormodel::InitialPose& pose) {
    const float speed = magnitude(particle.velocity);
    if (particle.motion == airborne) {
        particle.spin = particle.spin + (baseSpin + speedSpin * speed);
        if (speed > 0) {
            particle.forward = particle.velocity;
            normalize(particle.forward);
        }
        approachAngles(particle, directionAngles(particle.forward, pose), ordinaryMotion, pose);
    } else if (ordinaryMotion && particle.motion == rolling) {
        particle.spin = particle.spin + (baseSpin + speedSpin * speed);
        particle.forward[1] *= 0.7f;
        if (magnitude(particle.forward) > 0) normalize(particle.forward);
        particle.up[0] *= 0.5f;
        particle.up[1] *= 0.5f;
        particle.up[2] = std::min(particle.up[2] + 0.3f, 1.0f);
    } else if (ordinaryMotion && particle.motion == settling) {
        particle.up = {0, 0, 1};
        const auto direction = pose.horizontalDirection(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(particle.rotationPhase) + static_cast<std::uint16_t>(particle.angles[1])));
        particle.forward[0] = direction[0];
        particle.forward[2] = direction[2];
        if (particle.spin >= 0 && particle.spin < 90) particle.spin = std::min(particle.spin + settleStep, 90.0f);
        else if (particle.spin > 90 && particle.spin < 180) particle.spin = std::max(particle.spin - settleStep, 90.0f);
        else if (particle.spin >= 180 && particle.spin < 270) particle.spin = std::min(particle.spin + settleStep, 270.0f);
        else if (particle.spin > 270 && particle.spin <= 360) particle.spin = std::max(particle.spin - settleStep, 270.0f);
        if (particle.spin == 90 || particle.spin == 270) particle.motion = resting;
        normalize(particle.forward);
        particle.rotationPhase = static_cast<std::int16_t>(static_cast<std::uint16_t>(particle.rotationPhase) + particle.angularVelocity);
        const float factor = std::abs(int(particle.angularVelocity)) <= 10000 ? 1.03f : 1.0f;
        particle.angularVelocity = static_cast<std::int16_t>(static_cast<int>(particle.angularVelocity * factor));
    }
    if (particle.spin > 360) particle.spin -= 360;
}

bool moneyscatter::advance(Pool& pool, Request& request, Particle& particle, UpdateState& state,
    const actormodel::InitialPose& pose, UpdateServices& services, const std::optional<double>& retainedFoldedSpin) {
    state.ordinaryMotion = true;
    int suction = 0;
    if (request.suctionReady && particle.visible) suction = services.suction(100, particle);
    if (suction == 1) {
        accelerateSuction(particle, services);
        state.ordinaryMotion = false;
    } else if (suction == 2) {
        collectSuction(pool, request, particle, services);
        return false;
    } else if (suction == 0 && request.lifetime < 1000) {
        particle.lifetime = static_cast<std::int16_t>(static_cast<std::uint16_t>(particle.lifetime) - 1u);
    }
    move(particle, state.ordinaryMotion);
    CollisionResult contact;
    if (particle.motion < settling) contact = collide(particle, state.ordinaryMotion, pose, services, retainedFoldedSpin);
    advanceOrientation(particle, state.ordinaryMotion, pose);
    bool collected = false;
    if (request.collectionReady) collected = collect(request, particle, contact.wall, services);
    if (state.ordinaryMotion && contact.audible && !collected) services.bounceSound(particle.position);
    if (particle.sparkleCounter < sparklePeriod) {
        if (particle.visible) ++particle.sparkleCounter;
    } else {
        services.sparkle(particle.position);
        particle.sparkleCounter = 0;
    }
    const bool alive = particle.lifetime > 0;
    if (alive) services.updateModel(particle);
    else pool.recycle(request, particle);
    return alive;
}
