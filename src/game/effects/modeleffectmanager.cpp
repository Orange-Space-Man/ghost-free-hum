#include "resources/dolresources.h"
#include "game/effects/modeleffectmanager.h"
#include "game/effects/sharedmodeleffect.h"
#include "game/effects/cobwebeffectpool.h"
#include "game/effects/barriereffectpool.h"
#include "game/effects/bridgeeffectpool.h"
#include "game/player/playericeeffectpool.h"
#include "game/effects/icicleeffectpool.h"
#include "game/effects/icejaileffectpool.h"
#include "game/effects/waterfalleffectpool.h"
#include "game/startup/startupresources.h"

#include <stdexcept>
#include <utility>

modeleffects::Manager::Manager(Environment environment) : environment(std::move(environment)) {}

void modeleffects::Manager::add(Pool& pool) {
    if (registry.size() == 64) return;
    registry.push_back(&pool);
    factory.add(pool);
}

void modeleffects::Manager::initializeSpecialized(unsigned selector) {
    if (selector == 0x80172034) return;
    if (selector != 0x80172038 && selector != 0x80172074 && selector != 0x80172188) {
        throw std::runtime_error("Original model-effect map initializer is not implemented.");
    }
    specialized[0] = std::make_unique<SharedPool>();
    if (selector == 0x80172038) {
        add(*specialized[0]);
        return;
    }
    if (selector == 0x80172188) {
        if (!specialized[6]) specialized[6] = std::make_unique<PlayerIcePool>(environment.resources, environment.emitters);
        specialized[6]->owners[0]->reset();
        add(*specialized[0]);
        add(*specialized[6]);
        return;
    }
    specialized[1] = std::make_unique<CobwebPool>(environment.player, environment.particles);
    if (!specialized[2]) specialized[2] = std::make_unique<BarrierPool>(0, environment.emitters);
    if (!specialized[3]) specialized[3] = std::make_unique<BarrierPool>(1, environment.emitters);
    if (!specialized[4]) specialized[4] = std::make_unique<CrystalPool>(environment.particles, environment.randomState,
        environment.sound, environment.crystalScratch);
    if (!specialized[5]) specialized[5] = std::make_unique<BridgePool>();
    if (!specialized[6]) specialized[6] = std::make_unique<PlayerIcePool>(environment.resources, environment.emitters);
    for (unsigned i = 2; i <= 6; ++i) specialized[i]->owners[0]->reset();
    specialized[7] = std::make_unique<IciclePool>(10, 0, 0, 0x20);
    specialized[8] = std::make_unique<IciclePool>(12, 1, 40, 0x21);
    if (!specialized[9]) specialized[9] = std::make_unique<IceJailPool>();
    if (!specialized[10]) specialized[10] = std::make_unique<WaterfallPool>(environment.resources, environment.emitters,
        environment.flags, environment.sound);
    if (!specialized[11]) specialized[11] = std::make_unique<RiverPool>(environment.resources, environment.emitters,
        environment.elements, environment.ice, environment.water, environment.flags, environment.collision, environment.sound);
    for (unsigned i = 9; i < specialized.size(); ++i) specialized[i]->owners[0]->reset();
    for (auto& pool : specialized) add(*pool);
}

void modeleffects::Manager::initialize(const std::vector<std::uint8_t>& executable, unsigned value,
    particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    if (initialized) throw std::logic_error("Original model-effect manager must be destroyed before scene reinitialization.");
    if (value >= 14) throw std::out_of_range("Original model-effect map selector is invalid.");
    map = value;
    factory.clear();
    registry.clear();
    ordinary.clear();
    for (auto& buffer : drawBuffers) buffer = std::make_unique<j3dmodel::DrawBuffer>(1024);
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x80383fe0 + map * 4, 4, bytes, error)) throw std::runtime_error(error);
    const unsigned selector = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    initializeSpecialized(selector);
    for (auto& definition : readDefinitions(executable, map)) ordinary.push_back(std::make_unique<Pool>(std::move(definition)));
    for (auto& pool : ordinary) add(*pool);
    initialized = true;
    for (auto* pool : registry) pool->load(resources, defaults);
}

void modeleffects::Manager::clear() {
    if (!initialized) return;
    for (auto& buffer : drawBuffers) buffer.reset();
    for (auto* pool : registry) {
        for (auto& owner : pool->owners) owner->model.reset();
        pool->transform.reset();
        for (auto& group : pool->animations) group.clear();
    }
    ordinary.clear();
    if (map == 2) {
        for (unsigned i : {2u, 3u}) static_cast<BarrierPool&>(*specialized[i]).textureAnimation.reset();
        auto& crystal = static_cast<CrystalPool&>(*specialized[4]);
        crystal.colorAnimation.reset();
        crystal.textureAnimation.reset();
        auto& bridge = static_cast<BridgePool&>(*specialized[5]);
        bridge.colorAnimation.reset();
        bridge.textureAnimation.reset();
        auto& jail = static_cast<IceJailPool&>(*specialized[9]);
        jail.colorAnimation.reset();
        jail.textureAnimation.reset();
        static_cast<WaterfallPool&>(*specialized[10]).textureAnimation.reset();
        static_cast<RiverPool&>(*specialized[11]).textures.fill(nullptr);
        static_cast<RiverOwner&>(*specialized[11]->owners[0]).emitter = nullptr;
        static_cast<WaterfallOwner&>(*specialized[10]->owners[0]).emitters.fill(nullptr);
        specialized[8].reset();
        specialized[7].reset();
        specialized[1].reset();
    }
    if (map == 2 || map == 9) static_cast<PlayerIcePool&>(*specialized[6]).textureAnimation.reset();
    specialized[0].reset();
    registry.clear();
    factory.clear();
    initialized = false;
}

void modeleffects::Manager::update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility) {
    if (!initialized) throw std::logic_error("Original model-effect manager is not initialized.");
    for (auto* pool : registry) pool->update(rooms, visibility);
}

const std::vector<modeleffects::Pool*>& modeleffects::Manager::registered() const { return registry; }
