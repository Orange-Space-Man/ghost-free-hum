#include "leafscatterscene.h"
#include "scatterattraction.h"

#include <stdexcept>

leafscatter::Scene::Scene(const actormodel::InitialPose& pose, SceneDependencies dependencies)
    : pose(pose), dependencies(dependencies) {}
leafscatter::Scene::~Scene() { release(); }
void leafscatter::Scene::initialize(const std::shared_ptr<j3dmodel::ModelData>& model) { owner.initialize(model); }
void leafscatter::Scene::release() {
    if (auto* pool = owner.storage()) {
        for (unsigned i = 0; i < 15; ++i) dependencies.positions.release(&pool->requests.at(i).spawnPosition);
    }
    if (pendingSound) pendingSound->clearBinding();
    owner.release();
}
void leafscatter::Scene::scatter(const billscatter::Vector& position, const billscatter::Vector& direction,
    double strength, std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime) {
    owner.scatter(position, direction, strength, delay, count, lifetime, pose, *this);
}
void leafscatter::Scene::update() { owner.update(dependencies.paused, 30, dependencies.blinkThreshold, *this); }
leafscatter::Owner& leafscatter::Scene::storage() { return owner; }
std::uint32_t leafscatter::Scene::random() {
    auto& state = dependencies.gameplayRandom;
    state = state * 0x19660du + 0x3c6ef35fu;
    return state;
}
std::uint32_t leafscatter::Scene::roomAt(const billscatter::Vector& position, std::uint32_t previous) {
    return dependencies.rooms.resolve(position, previous);
}
std::uintptr_t leafscatter::Scene::startSound(unsigned id, const billscatter::Vector& position) {
    dependencies.audio.play(id, position, &pendingSound);
    return reinterpret_cast<std::uintptr_t>(pendingSound);
}
void leafscatter::Scene::soundCount(std::uintptr_t handle, std::uint8_t count) {
    if (!pendingSound || reinterpret_cast<std::uintptr_t>(pendingSound) != handle)
        throw std::runtime_error("Leaf spawn lost its original sound handle.");
    dependencies.audio.setPort(*pendingSound, 8, count);
    pendingSound->clearBinding();
}
std::uint16_t leafscatter::Scene::visibilityFlags(std::uint32_t room) { return dependencies.visibility.record(room, 0).flags; }
mapcollision::SphereContact leafscatter::Scene::sweep(const billscatter::Vector& position, float radius, const billscatter::Vector& velocity) {
    return dependencies.collision.sweepSphere(position, radius, velocity);
}
int leafscatter::Scene::suction(billscatter::Particle& particle) {
    return scattraction::apply(particle, dependencies.suctionMode, dependencies.playerContext, dependencies.suctionRange, dependencies.gameplayRandom);
}
billscatter::Vector leafscatter::Scene::suctionPosition() {
    if (!dependencies.playerContext.suction.position) throw std::runtime_error("Leaf suction context is unavailable.");
    return *dependencies.playerContext.suction.position;
}
billscatter::Vector leafscatter::Scene::playerPosition() { return dependencies.playerPosition; }
float leafscatter::Scene::playerTop() { return dependencies.playerContext.height; }
bool leafscatter::Scene::advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle) {
    return leafscatter::advance(pool, request, particle, owner.color(), dependencies.flutterStrength,
        dependencies.particleRandom, dependencies.gameplayRandom, pose, *this);
}
void leafscatter::Scene::draw(RenderServices& services) {
    auto* pool = owner.storage();
    if (!pool) return;
    for (auto* request = pool->active.sentinel.next; request != &pool->active.sentinel; request = request->next)
        for (auto* particle = request->particles.sentinel.next; particle != &request->particles.sentinel; particle = particle->next)
            if (particle->visible) services.draw(*particle->model);
}
void leafscatter::Scene::view(const j3dmodel::Matrix& camera, RenderServices& services) {
    auto* pool = owner.storage();
    if (!pool) return;
    for (auto* request = pool->active.sentinel.next; request != &pool->active.sentinel; request = request->next)
        for (auto* particle = request->particles.sentinel.next; particle != &request->particles.sentinel; particle = particle->next)
            if (particle->visible) services.view(*particle->model, camera);
}
