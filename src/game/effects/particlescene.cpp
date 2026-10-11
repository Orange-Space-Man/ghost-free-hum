#include "game/effects/particlescene.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>

struct particles::Scene::Binding {
    Binding(playeractor::Player& player, enemies::Manager& manager, const actormodel::InitialPose& pose,
        const roomtraversal::State& visibility, SceneContexts& contexts, std::uint32_t& randomState) :
        player(player), manager(manager), pose(pose), visibility(visibility),
        suction(player.view, contexts.player.suction, [&randomState] {
            randomState = randomState * 0x19660d + 0x3c6ef35f;
            return randomState;
        }), callback(suction) {
    }

    playeractor::Player& player;
    enemies::Manager& manager;
    const actormodel::InitialPose& pose;
    const roomtraversal::State& visibility;
    Suction suction;
    SuctionCallback callback;
    bool refreshed = false;
};

particles::Scene::Scene(const std::vector<std::uint8_t>& executable, ArchiveResources& resources,
    std::uint32_t& randomState) : randomState(randomState), emitters(128, 1024, 1024),
    factory(executable, resources, emitters), simulation(executable, randomState), executable(executable), resources(resources) {
}

particles::Scene::~Scene() = default;

void particles::Scene::bind(enemies::Manager& manager, actors::Registry& actors, const actormodel::InitialPose& pose,
    const roomtraversal::State& visibility) {
    if (binding) throw std::logic_error("Particle scene is already bound to its original player owner.");
    if (!manager.actorsInitialized()) throw std::runtime_error("Particle scene binding requires initialized EnManager actors.");
    auto* player = dynamic_cast<playeractor::Player*>(actors.actor(manager.appearPoint(0).actor));
    if (!player) throw std::runtime_error("Particle scene requires the original player-zero actor.");
    binding = std::make_unique<Binding>(*player, manager, pose, visibility, shared, randomState);
}

void particles::Scene::refresh(const SecondarySource* secondary) {
    if (!binding) throw std::runtime_error("Particle scene context has no player binding.");
    shared.refresh(binding->player.view, binding->pose, binding->manager, binding->visibility, secondary);
    binding->refreshed = true;
}

void particles::Scene::update() { emitters.update(simulation); }

void particles::Scene::initializeElements() {
    if (elementalScene) throw std::logic_error("Elemental scene is already initialized.");
    elementalScene = std::make_unique<elements::Scene>(executable);
}

void particles::Scene::initializeEffectBatches() {
    if (batches[0]) throw std::logic_error("Original particle batches are already initialized.");
    for (unsigned i = 0; i < batches.size(); ++i) batches[i] = std::make_unique<Batch>(executable, resources, emitters, 10, i);
}

void particles::Scene::updateElements() {
    if (!binding || !binding->refreshed || !elementalScene || !batches[1] || !batches[2]) {
        throw std::runtime_error("Elemental scene update requires initialized storage and the original refreshed player context.");
    }
    elementalScene->updateProjectiles(binding->player.view, binding->player.flags);
    batches[1]->update();
    batches[2]->update();
    elementalScene->update(binding->player.view, binding->player.flags, shared.player, binding->visibility);
}

elements::Scene& particles::Scene::elemental() {
    if (!elementalScene) throw std::logic_error("Elemental scene is not initialized.");
    return *elementalScene;
}

particles::Batch& particles::Scene::batch(unsigned index) {
    auto& result = batches.at(index);
    if (!result) throw std::logic_error("Original particle batch is not initialized.");
    return *result;
}

particles::Emitter* particles::Scene::createDamage(unsigned type, const std::array<float, 3>& position) {
    if (!binding || !binding->refreshed) throw std::runtime_error("Damage particles require the original refreshed player context.");
    return factory.createDamage(type, position, binding->callback);
}

particles::SceneContexts& particles::Scene::contexts() { return shared; }
particles::EmitterPool& particles::Scene::pool() { return emitters; }
particles::EffectFactory& particles::Scene::effects() { return factory; }
