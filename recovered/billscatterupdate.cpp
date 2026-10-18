#include "billscatterupdate.h"
#include "moneyscatterinitializer.h"

#include <algorithm>
#include <cmath>

namespace {
    float magnitude(const billscatter::Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }
}

void billscatter::advanceOrientation(Particle& particle, bool ordinaryMotion, const actormodel::InitialPose& pose) {
    const float speed = magnitude(particle.velocity);
    unsigned step = 1000;
    if (particle.motion == 0) {
        if (speed > 0) {
            const float inverse = 1.0f / speed;
            for (unsigned i = 0; i < 3; ++i) particle.forward[i] = particle.velocity[i] * inverse;
        }
        particle.spin = (particle.spin + 10.0f) + 1.2f * speed;
    } else if (ordinaryMotion) {
        particle.acceleration = {};
        particle.velocity[1] = 0;
        if (particle.spin >= 0 && particle.spin < 90) particle.spin = std::max(particle.spin - 15.0f, 0.0f);
        else if (particle.spin > 90 && particle.spin < 180) particle.spin = std::min(particle.spin + 15.0f, 180.0f);
        else if (particle.spin >= 180 && particle.spin < 270) particle.spin = std::max(particle.spin - 15.0f, 180.0f);
        else if (particle.spin > 270 && particle.spin <= 360) particle.spin = std::min(particle.spin + 15.0f, 360.0f);
        step = 1500;
    }
    if (particle.spin >= 360) particle.spin -= 360;
    if (!ordinaryMotion) step *= 20;
    const auto target = moneyscatter::directionAngles(particle.forward, pose);
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned current = static_cast<std::uint16_t>(particle.angles[i]);
        const unsigned difference = (static_cast<std::uint16_t>(target[i]) - current) & 0xffff;
        const unsigned next = difference < 0x8000 ? current + std::min(difference, step) :
            current - std::min(0xffff - difference, step);
        particle.angles[i] = static_cast<std::int16_t>(next);
    }
    const auto matrix = pose.rotation(particle.angles);
    particle.forward = {matrix[2], matrix[6], matrix[10]};
    const float length = magnitude(particle.forward);
    if (length >= 0.0001f) {
        const float inverse = static_cast<float>(1.0 / length);
        for (auto& component : particle.forward) component *= inverse;
    } else particle.forward = {};
}

bool billscatter::advance(Pool& pool, Request& request, Particle& particle, float flutterStrength,
    std::uint8_t flutterThreshold, std::uint32_t& particleRandom, std::uint32_t& gameplayRandom,
    const actormodel::InitialPose& pose, UpdateServices& services) {
    int suction = 0;
    if (request.suctionReady && particle.visible) suction = services.suction(particle);
    const bool ordinaryMotion = suction != 1;
    if (suction == 1) {
        scaleSuction(particle, services.suctionPosition());
        particle.motion = 0;
    } else if (suction == 2) {
        collectSuction(pool, request, particle, services);
        return false;
    } else if (suction == 0) {
        particle.lifetime = static_cast<std::int16_t>(static_cast<std::uint16_t>(particle.lifetime) - 1u);
    }
    move(particle, request, ordinaryMotion, flutterStrength, flutterThreshold, particleRandom, gameplayRandom, pose, services);
    advanceOrientation(particle, ordinaryMotion, pose);
    collect(particle, services);
    const bool alive = particle.lifetime > 0;
    if (alive) services.updateModel(particle);
    else pool.recycle(request, particle);
    return alive;
}
