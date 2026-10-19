#include "leafscatterinitializer.h"
#include "billscattermodels.h"
#include "moneyscatterinitializer.h"

namespace {
    template<class Item>
    Item* takeLast(particles::Ring<Item>& ring) {
        if (!ring.count) return nullptr;
        auto* item = ring.sentinel.previous;
        ring.remove(*item);
        return item;
    }

    float unit(leafscatter::InitializationServices& services) {
        return static_cast<float>(services.random()) / 4294967296.0f;
    }

    void initialize(billscatter::Request& request, billscatter::Particle& particle,
        const actormodel::InitialPose& pose, leafscatter::InitializationServices& services) {
        particle.velocity = {};
        particle.acceleration = {};
        particle.position = request.spawnPosition;
        constexpr float sizeVariation = 60;
        particle.sizeRatio = static_cast<std::uint8_t>(static_cast<int>((255.0f - sizeVariation) - sizeVariation * unit(services)));
        particle.angles = moneyscatter::directionAngles(request.direction, pose);
        particle.lifetime = request.lifetime;
        particle.scale = (0.75f * particle.sizeRatio) / 255.0f;
        particle.blinking = false;
        particle.blinkCounter = 0;
        particle.forward = {0, -1, 0};
        particle.flutterCounter = static_cast<std::uint8_t>(static_cast<unsigned>(4.0f * unit(services)));
        particle.spin = 360.0f * unit(services);
        particle.room = services.roomAt(particle.position, 0xffffffff);
        particle.currentRoom = particle.room;
        particle.motion = 0;
        particle.up = {0, 0, 1};
        request.particles.append(particle);
        billscatter::updateModel(particle);
    }
}

billscatter::Request* leafscatter::scatter(billscatter::Pool& pool, const billscatter::Vector& position,
    const billscatter::Vector& direction, double strength, std::uint8_t delay, std::uint8_t count,
    std::uint16_t lifetime, const actormodel::InitialPose& pose, InitializationServices& services) {
    auto* request = takeLast(pool.requests.free);
    if (!request) return nullptr;
    request->position = position;
    request->direction = direction;
    for (unsigned i = 0; i < 3; ++i) request->spawnPosition[i] = position[i] + 5.0f * direction[i];
    request->count = count;
    request->lifetime = static_cast<std::int16_t>(lifetime);
    request->strength = static_cast<float>(strength);
    request->suctionReady = false;
    request->suctionCounter = 0;
    request->delay = delay;
    const auto sound = services.startSound(0x186e, request->spawnPosition);
    if (sound) services.soundCount(sound, count);
    for (unsigned i = 0; i < count; ++i) {
        auto* particle = takeLast(pool.particles.free);
        if (!particle) break;
        initialize(*request, *particle, pose, services);
    }
    pool.active.append(*request);
    return request;
}
