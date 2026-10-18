#include "moneyscatterowner.h"

moneyscatter::Owner::Owner(Timing timing, const std::shared_ptr<j3dmodel::ModelData>& coin,
    const actormodel::InitialPose& pose, SceneDependencies dependencies, float retainedAuxiliarySpin) :
    bindings(dependencies), pool(timing, false), models(pool, coin, retainedAuxiliarySpin), pose(pose), sparkles(dependencies.sparkles) {
    pool.initializePersistent();
}

moneyscatter::Request* moneyscatter::Owner::scatter(const Vector& position, const Vector& direction, std::uint8_t count,
    std::uint16_t lifetime, double speed, std::uint8_t positionMode) {
    return pool.scatter(position, direction, count, lifetime, speed, positionMode, *this);
}

void moneyscatter::Owner::updateCoins(const Vector& position, const Vector& direction) {
    cameraPosition = position;
    cameraDirection = direction;
    sparkles.update();
    pool.update(*this);
}

void moneyscatter::Owner::initializeParticle(Request& request, Particle& particle) {
    initialize(request, particle, pose, bindings);
}

std::uint32_t moneyscatter::Owner::roomAt(const Vector& position, std::uint32_t previousRoom) {
    return bindings.roomAt(position, previousRoom);
}

std::uint16_t moneyscatter::Owner::roomFlags(std::uint32_t room) { return bindings.visibilityFlags(room); }

bool moneyscatter::Owner::advanceParticle(Pool& pool, Request& request, Particle& particle) {
    return advance(pool, request, particle, state, pose, bindings);
}

void moneyscatter::Owner::updateAuxiliaryModel() { models.updateAuxiliary(cameraPosition, cameraDirection); }
moneyscatter::Pool& moneyscatter::Owner::storage() { return pool; }
const moneyscatter::Models& moneyscatter::Owner::modelStorage() const { return models; }
