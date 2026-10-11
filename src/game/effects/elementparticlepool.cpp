#include "game/effects/elementparticlepool.h"

#include <stdexcept>

namespace {
    unsigned particleCount(unsigned count) {
        count &= 0xffff;
        if (!count) throw std::invalid_argument("Original elemental particle pool requires nonzero capacity.");
        return count;
    }

    unsigned emitterCount(unsigned count) {
        count &= 0xffff;
        if (!count || count > 256) throw std::invalid_argument("Original elemental emitter count exceeds its linking loop.");
        return count;
    }
}

elements::Particle::Particle() : next(this), previous(this) {}

elements::Emitter::Emitter() : next(this), previous(this) {}

elements::Pool::Pool(unsigned particleCapacity, unsigned emitterCapacity)
    : particles(particleCount(particleCapacity)), emitters(emitterCount(emitterCapacity)) {}

bool elements::Pool::intersects(const Emitter& emitter, const actorcollision::Shape& target,
    const actorcollision::Vector& position) const {
    const actorcollision::Shape source{2, nullptr, &particleBox};
    actorcollision::Vector displacement{};
    for (auto* particle = emitter.particles.sentinel.next; particle != &emitter.particles.sentinel; particle = particle->next) {
        if (particle->age > 10 && actorcollision::intersect(displacement, source, particle->position, target, position)) return true;
    }
    return false;
}

bool elements::Pool::intersects(const actorcollision::Shape& target, const actorcollision::Vector& position, int type) const {
    if (type <= 2) return false;
    for (auto* emitter = active.sentinel.next; emitter != &active.sentinel; emitter = emitter->next) {
        if (emitter->type == type) return intersects(*emitter, target, position);
    }
    return false;
}

bool elements::intersects(const Projectile& ice, const Projectile& water, const actorcollision::Shape& target,
    const actorcollision::Vector& position, int type) {
    const auto* projectile = type == 10 ? &ice : type == 11 ? &water : nullptr;
    if (!projectile || projectile->active != 1) return false;
    const actorcollision::Box box{{200, 200, 200}, {}};
    const actorcollision::Shape source{2, nullptr, &box};
    actorcollision::Vector displacement{};
    return actorcollision::intersect(displacement, source, projectile->position, target, position);
}
