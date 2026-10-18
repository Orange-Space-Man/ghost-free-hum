#include "moneyscatterpool.h"
#include <stdexcept>

namespace {
    template<class Item>
    Item* takeLast(particles::Ring<Item>& ring) {
        if (!ring.count) return nullptr;
        auto* item = ring.sentinel.previous;
        ring.remove(*item);
        return item;
    }

    std::int16_t decrement(std::int16_t value) {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(value) - 1u);
    }
}

moneyscatter::Pool::Pool(Timing timing, bool reservePersistent) : timing(timing), retained(nullptr) {
    if (reservePersistent) initializePersistent();
}

void moneyscatter::Pool::initializePersistent() {
    if (retained) throw std::logic_error("Persistent money request is already initialized.");
    retained = takeLast(requests.free);
    if (!retained) throw std::runtime_error("Persistent money initialization requires an available request.");
    retained->lifetime = 1000;
    retained->direction = {};
    retained->count = 1;
    retained->speed = 0;
    retained->suctionReady = retained->collectionReady = false;
    retained->suctionCounter = retained->collectionCounter = 0;
    retained->positionMode = 0;
    retained->placedMoney = true;
    active.append(*retained);
}

moneyscatter::Request* moneyscatter::Pool::scatter(const Vector& position, const Vector& direction,
    std::uint8_t count, std::uint16_t lifetime, double speed, std::uint8_t positionMode, Services& services) {
    auto* request = takeLast(requests.free);
    if (!request) return nullptr;
    request->position = position;
    request->lifetime = static_cast<std::int16_t>(lifetime);
    request->direction = direction;
    request->count = count;
    request->speed = static_cast<float>(speed);
    request->suctionReady = request->collectionReady = false;
    request->suctionCounter = request->collectionCounter = 0;
    request->positionMode = positionMode;
    request->placedMoney = false;
    for (unsigned index = 0; index < count; ++index) {
        auto* particle = takeLast(particles.free);
        if (!particle) break;
        services.initializeParticle(*request, *particle);
        request->particles.append(*particle);
    }
    active.append(*request);
    return request;
}

void moneyscatter::Pool::recycle(Request& request, Particle& particle) {
    request.particles.remove(particle);
    particles.free.append(particle);
}

bool moneyscatter::Pool::updateRequest(Request& request, Services& services) {
    if (!request.particles.count) {
        if (request.lifetime < 1000) {
            active.remove(request);
            requests.free.append(request);
        }
        return false;
    }
    for (auto* particle = request.particles.sentinel.next; particle != &request.particles.sentinel;) {
        auto* next = particle->next;
        particle->currentRoom = services.roomAt(particle->position, particle->room);
        particle->visible = (services.roomFlags(particle->currentRoom) & 1) != 0;
        if (request.lifetime < 1000) {
            if (particle->blinking) {
                if (particle->blinkCounter == timing.blinkThreshold) {
                    particle->visible = false;
                    particle->blinkCounter = 0;
                }
                ++particle->blinkCounter;
            }
            if (particle->lifetime <= 50) particle->blinking = true;
        }
        if (services.advanceParticle(*this, request, *particle)) next = particle->next;
        particle = next;
    }
    return true;
}

void moneyscatter::Pool::update(Services& services) {
    if (paused) return;
    for (auto* request = active.sentinel.next; request != &active.sentinel;) {
        auto* next = request->next;
        if (request->lifetime < 1000) request->lifetime = decrement(request->lifetime);
        if (request->suctionCounter < timing.suctionDelay) ++request->suctionCounter;
        else request->suctionReady = true;
        if (request->collectionCounter < timing.collectionDelay) ++request->collectionCounter;
        else request->collectionReady = true;
        if (updateRequest(*request, services)) next = request->next;
        request = next;
    }
    services.updateAuxiliaryModel();
}

moneyscatter::Request& moneyscatter::Pool::persistent() {
    if (!retained) throw std::logic_error("Persistent money request is not initialized.");
    return *retained;
}
