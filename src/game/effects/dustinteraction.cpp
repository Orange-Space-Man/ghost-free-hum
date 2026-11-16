#include "game/effects/dustinteraction.h"
#include "resources/dolresources.h"

#include <algorithm>
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
        std::memcpy(&result, &bits, 4);
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

    void release(runningdust::InteractionParticle& particle) {
        if (particle.forced == 1) {
            particle.forced = 0;
            particle.acceleration = {};
        }
    }
}

runningdust::ForceMode runningdust::selectForce(bool overrideSuction, bool emitterOutward, bool vacuumPresent, int playerMode) {
    if (overrideSuction) return ForceMode::Suction;
    if (emitterOutward) return ForceMode::Outward;
    if (vacuumPresent) return ForceMode::Suction;
    return playerMode == 6 ? ForceMode::Outward : ForceMode::None;
}

runningdust::Interaction::Interaction(const std::vector<std::uint8_t>& executable) : motion(executable),
    captureRadius(read(executable, 0x804dc28c)), outwardNearRadius(read(executable, 0x804dc290)) {}

runningdust::ForceResult runningdust::Interaction::advance(const ForceFrame& frame,
    const InteractionEmitter& emitter, InteractionParticle& particle) const {
    if (frame.mode == ForceMode::None) {
        release(particle);
        if (particle.influenced == 1) {
            if (emitter.type == 0x11) particle.lifetime = std::min<std::int16_t>(particle.lifetime, 200);
            else if (emitter.type == 1) particle.lifetime = std::min(particle.lifetime, emitter.releasedLifetime);
        }
        return particle.forced ? ForceResult::Forced : ForceResult::Ordinary;
    }
    if (frame.mode == ForceMode::Suction && particle.lifetime >= emitter.captureLifetime)
        return particle.forced ? ForceResult::Forced : ForceResult::Ordinary;
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = particle.position[i] - frame.origin[i];
    const float distance = magnitude(delta);
    if (frame.mode == ForceMode::Suction && distance <= captureRadius) {
        if (!frame.capture) throw std::runtime_error("Dust capture requires its original event service.");
        frame.capture(emitter.type == 1 ? 1 : 0, emitter.level);
        return ForceResult::Removed;
    }
    if (distance <= frame.radius) {
        auto direction = normalized(delta);
        const float dot = std::fma(direction[0], frame.axis[0], direction[1] * frame.axis[1]) + direction[2] * frame.axis[2];
        if (frame.cosine < dot || (frame.mode == ForceMode::Outward && distance < outwardNearRadius)) {
            particle.forced = 1;
            particle.influenced = 1;
            if (frame.mode == ForceMode::Suction) {
                for (auto& component : direction) component = -component;
                motion.advance(distance, frame.strength, frame.origin, frame.axis, direction, emitter.motion, particle, frame.random);
            } else motion.outward(distance, frame.strength, direction, emitter.motion, particle, frame.randomDirection);
            return ForceResult::Forced;
        }
    }
    release(particle);
    return particle.forced ? ForceResult::Forced : ForceResult::Ordinary;
}
