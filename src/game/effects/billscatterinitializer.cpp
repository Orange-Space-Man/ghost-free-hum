#include "billscatterinitializer.h"

namespace {
    template<class Item>
    Item* takeLast(particles::Ring<Item>& ring) {
        if (!ring.count) return nullptr;
        auto* item = ring.sentinel.previous;
        ring.remove(*item);
        return item;
    }

    float unit(billscatter::InitializationServices& services) {
        return static_cast<float>(services.random()) / 4294967296.0f;
    }
}

void billscatter::initialize(Request& request, Particle& particle, std::uint8_t flutterThreshold,
    InitializationServices& services) {
    particle.velocity = {};
    particle.acceleration = {};
    particle.position = request.spawnPosition;
    particle.forward = request.direction;
    particle.angles = {0, 0x7fff, 0};
    particle.lifetime = request.lifetime;
    particle.scale = 0.75f;
    particle.blinking = false;
    particle.blinkCounter = 0;
    particle.flutterCounter = static_cast<std::uint8_t>(
        static_cast<unsigned>(static_cast<float>(flutterThreshold + 1) * unit(services)));
    particle.spin = 360.0f * unit(services);
    particle.room = services.roomAt(particle.position, 0xffffffff);
    particle.currentRoom = particle.room;
    particle.motion = 0;
    particle.up = {0, 0, 1};
    services.updateModel(particle);
    request.particles.append(particle);
}

billscatter::Request* billscatter::scatter(Pool& pool, const Vector& position, const Vector& direction,
    double strength, std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
    std::uint8_t flutterThreshold, InitializationServices& services) {
    auto* request = takeLast(pool.requests.free);
    if (!request) return nullptr;
    request->position = position;
    request->direction = direction;
    for (unsigned i = 0; i < 3; ++i) request->spawnPosition[i] = position[i] + direction[i] * 5.0f;
    request->count = count;
    request->lifetime = static_cast<std::int16_t>(lifetime);
    request->strength = static_cast<float>(strength);
    request->suctionReady = false;
    request->suctionCounter = 0;
    request->delay = delay;
    for (unsigned i = 0; i < count; ++i) {
        auto* particle = takeLast(pool.particles.free);
        if (!particle) break;
        initialize(*request, *particle, flutterThreshold, services);
    }
    pool.active.append(*request);
    return request;
}

void billscatter::scatter(Owners& owners, const Vector& position, const Vector& direction, double strength,
    std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
    std::uint8_t flutterThreshold, InitializationServices& services) {
    for (unsigned i = 0; i < count % 3; ++i) {
        const unsigned variant = static_cast<unsigned>(3.0f * unit(services)) & 0xff;
        if (variant < owners.pools.size())
            scatter(*owners.pools[variant], position, direction, strength, delay, 1, lifetime, flutterThreshold, services);
    }
    if (count / 3) {
        for (auto& pool : owners.pools)
            scatter(*pool, position, direction, strength, delay, count / 3, lifetime, flutterThreshold, services);
    }
}
