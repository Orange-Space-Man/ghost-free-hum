#include "leafscatterowner.h"

void leafscatter::Owner::initialize(const std::shared_ptr<j3dmodel::ModelData>& model) {
    if (pool) return;
    pool = std::make_unique<billscatter::Pool>(model, 50);
    channels = {255, 255, 255};
}

void leafscatter::Owner::release() { pool.reset(); }

billscatter::Request* leafscatter::Owner::scatter(const billscatter::Vector& position, const billscatter::Vector& direction,
    double strength, std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
    const actormodel::InitialPose& pose, InitializationServices& services) {
    if (!pool) return nullptr;
    return leafscatter::scatter(*pool, position, direction, strength, delay, count, lifetime, pose, services);
}

void leafscatter::Owner::update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold,
    billscatter::Services& services) {
    if (pool) pool->update(paused, suctionDelay, blinkThreshold, services);
}

bool leafscatter::Owner::active() const { return bool(pool); }
billscatter::Pool* leafscatter::Owner::storage() { return pool.get(); }
const std::array<std::uint16_t, 3>& leafscatter::Owner::color() const { return channels; }
