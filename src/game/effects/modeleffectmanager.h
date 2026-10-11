#pragma once

#include "game/effects/crystaleffectpool.h"
#include "game/effects/rivereffectpool.h"
#include "jlib/j3d/j3ddrawbuffer.h"
#include "game/player/playeractorview.h"

namespace modeleffects {
    struct Environment {
        particles::ArchiveResources& resources;
        particles::EmitterPool& emitters;
        particles::EffectFactory& particles;
        const playeractor::View& player;
        const elements::Pool& elements;
        const elements::Projectile& ice;
        const elements::Projectile& water;
        gameflags::State& flags;
        mapcollision::State& collision;
        std::uint32_t& randomState;
        CrystalPool::Sound sound;
        CrystalPool::Scratch crystalScratch;
    };

    class Manager {
    public:
        explicit Manager(Environment environment);
        void initialize(const std::vector<std::uint8_t>& executable, unsigned map, particles::ArchiveResources& resources,
            const j3dmodel::MaterialDefaults& defaults);
        void clear();
        void update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility);
        const std::vector<Pool*>& registered() const;
        Factory factory;
        std::array<std::unique_ptr<j3dmodel::DrawBuffer>, 2> drawBuffers;

    private:
        Environment environment;
        unsigned map = 0;
        bool initialized = false;
        std::array<std::unique_ptr<Pool>, 12> specialized;
        std::vector<std::unique_ptr<Pool>> ordinary;
        std::vector<Pool*> registry;
        void add(Pool& pool);
        void initializeSpecialized(unsigned selector);
    };
}
