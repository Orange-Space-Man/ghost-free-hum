#pragma once

#include "game/effects/particlescenecontext.h"
#include "game/effects/particlesimulation.h"
#include "game/effects/particleeffectfactory.h"
#include "game/effects/particlebatch.h"
#include "game/effects/elementparticlescene.h"

namespace actors { class Registry; }

namespace particles {
    class Scene {
    public:
        Scene(const std::vector<std::uint8_t>& executable, ArchiveResources& resources, std::uint32_t& randomState);
        ~Scene();
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        void bind(enemies::Manager& manager, actors::Registry& actors, const actormodel::InitialPose& pose,
            const roomtraversal::State& visibility);
        void refresh(const SecondarySource* secondary);
        void update();
        void initializeElements();
        void initializeEffectBatches();
        void updateElements();
        elements::Scene& elemental();
        Batch& batch(unsigned index);
        Emitter* createDamage(unsigned type, const std::array<float, 3>& position);
        SceneContexts& contexts();
        EmitterPool& pool();
        EffectFactory& effects();

    private:
        struct Binding;
        std::uint32_t& randomState;
        SceneContexts shared;
        std::unique_ptr<Binding> binding;
        EmitterPool emitters;
        EffectFactory factory;
        NativeSimulation simulation;
        const std::vector<std::uint8_t>& executable;
        ArchiveResources& resources;
        std::unique_ptr<elements::Scene> elementalScene;
        std::array<std::unique_ptr<Batch>, 3> batches;
    };
}
