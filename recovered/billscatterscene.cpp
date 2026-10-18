#include "billscatterscene.h"
#include "billscattermodels.h"

#include <stdexcept>
#include <utility>

namespace {
    constexpr std::uint8_t flutterThreshold = 3, suctionDelay = 30;
}

billscatter::Scene::Scene(const std::array<std::shared_ptr<j3dmodel::ModelData>, 3>& models,
    const actormodel::InitialPose& pose, SceneDependencies dependencies)
    : dependencies(std::move(dependencies)), pose(pose), owners(models) {
    if (!this->dependencies.dispatch || !this->dependencies.attraction)
        throw std::runtime_error("Bill scene requires original player dispatch and attraction routes.");
}

void billscatter::Scene::scatter(const Vector& position, const Vector& direction, double strength,
    std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime) {
    billscatter::scatter(owners, position, direction, strength, delay, count, lifetime, flutterThreshold, *this);
}

void billscatter::Scene::update() { owners.update(dependencies.paused, suctionDelay, dependencies.blinkThreshold, *this); }
billscatter::Owners& billscatter::Scene::storage() { return owners; }

std::uint32_t billscatter::Scene::random() {
    auto& state = dependencies.gameplayRandom;
    state = state * 0x19660du + 0x3c6ef35fu;
    return state;
}

std::uint32_t billscatter::Scene::roomAt(const Vector& position, std::uint32_t previous) {
    return dependencies.rooms.resolve(position, previous);
}
std::uint16_t billscatter::Scene::visibilityFlags(std::uint32_t room) { return dependencies.visibility.record(room, 0).flags; }
mapcollision::SphereContact billscatter::Scene::sweep(const Vector& position, float radius, const Vector& velocity) {
    return dependencies.collision.sweepSphere(position, radius, velocity);
}
billscatter::Vector billscatter::Scene::playerPosition() { return dependencies.playerPosition; }
float billscatter::Scene::playerTop() { return dependencies.playerContext.height; }
void billscatter::Scene::playerEvent(unsigned event, const playerevents::Payload& payload) { dependencies.dispatch(event, payload); }
int billscatter::Scene::suction(Particle& particle) { return dependencies.attraction(particle); }
billscatter::Vector billscatter::Scene::suctionPosition() {
    if (!dependencies.playerContext.suction.position)
        throw std::runtime_error("Bill suction requires the refreshed original scene position.");
    return *dependencies.playerContext.suction.position;
}
void billscatter::Scene::updateModel(Particle& particle) { billscatter::updateModel(particle); }
bool billscatter::Scene::advance(Pool& pool, Request& request, Particle& particle) {
    return billscatter::advance(pool, request, particle, dependencies.flutterStrength, flutterThreshold,
        dependencies.particleRandom, dependencies.gameplayRandom, pose, *this);
}
