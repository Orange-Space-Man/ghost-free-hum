#pragma once

#include "game/effects/particlescenecontext.h"
#include "game/effects/particlesimulation.h"
#include "game/effects/particleeffectfactory.h"
#include "game/effects/particlebatch.h"
#include "game/effects/elementparticlescene.h"
#include "game/effects/particlebillboards.h"
#include "game/effects/runningdustmanager.h"

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
        std::vector<Billboard> prepareGlows(GlowPool& glows, const std::array<float, 12>& view);
        std::vector<Billboard> prepareActorGlows(const actors::Registry& actors, const roomtraversal::State& visibility,
            const std::array<float, 12>& view, unsigned buffer) const;
        void initializeElements();
        void initializeRunningDust(const mission::Resources& mission, const roomlocation::Rooms& rooms);
        runningdust::Manager& runningDust();
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
        std::unique_ptr<runningdust::Manager> dust;
        std::array<std::unique_ptr<Batch>, 3> batches;
    };
}
