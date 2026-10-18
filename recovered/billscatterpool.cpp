#include "billscatterpool.h"

billscatter::Pool::Pool(const std::shared_ptr<j3dmodel::ModelData>& model) {
    for (unsigned i = 0; i < 30; ++i)
        particles.at(i).model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
}

void billscatter::Pool::recycle(Request& request, Particle& particle) {
    request.particles.remove(particle);
    particles.free.append(particle);
}

bool billscatter::Pool::updateRequest(Request& request, std::uint8_t blinkThreshold, Services& services) {
    if (!request.particles.count) {
        active.remove(request);
        requests.free.append(request);
        return false;
    }
    for (auto* particle = request.particles.sentinel.next; particle != &request.particles.sentinel;) {
        auto* next = particle->next;
        particle->currentRoom = services.roomAt(particle->position, particle->room);
        particle->visible = (services.visibilityFlags(particle->currentRoom) & 1) != 0;
        if (particle->blinking) {
            if (particle->blinkCounter == blinkThreshold) {
                particle->visible = false;
                particle->blinkCounter = 0;
            }
            ++particle->blinkCounter;
        }
        if (particle->lifetime <= 50) particle->blinking = true;
        if (services.advance(*this, request, *particle)) next = particle->next;
        particle = next;
    }
    return true;
}

void billscatter::Pool::update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold, Services& services) {
    if (paused) return;
    for (auto* request = active.sentinel.next; request != &active.sentinel;) {
        auto* next = request->next;
        request->lifetime = static_cast<std::int16_t>(static_cast<std::uint16_t>(request->lifetime) - 1u);
        if (request->delay) --request->delay;
        if (request->suctionCounter < suctionDelay) ++request->suctionCounter;
        else request->suctionReady = true;
        if (updateRequest(*request, blinkThreshold, services)) next = request->next;
        request = next;
    }
}

billscatter::Owners::Owners(const std::array<std::shared_ptr<j3dmodel::ModelData>, 3>& models) {
    for (unsigned i = 0; i < pools.size(); ++i) pools[i] = std::make_unique<Pool>(models[i]);
}

void billscatter::Owners::update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold, Services& services) {
    for (auto& pool : pools) pool->update(paused, suctionDelay, blinkThreshold, services);
}
