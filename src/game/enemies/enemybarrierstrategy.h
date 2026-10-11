#pragma once

#include "game/enemies/enemystrategydecorator.h"
#include "game/effects/barriereffectpool.h"
#include "game/world/doorstate.h"
#include "game/player/playeractor.h"

namespace enemystrategy {
    struct BarrierEnvironment {
        doors::State& doors;
        modeleffects::Factory& models;
        particles::ArchiveResources& common;
        enemies::Runtime& runtime;
        const playeractor::View& player;
    };

    class Barrier final : public Decorator, public characterevent::Observer {
    public:
        explicit Barrier(BarrierEnvironment& environment);
        ~Barrier() override;
        void begin() override;
        void initializeBehavior() override;
        void behavior() override;
        unsigned doorId() const;

    private:
        unsigned playerArea() const;
        BarrierEnvironment& environment;
        modeleffects::BarrierOwner* effect = nullptr;
        unsigned door = 0, area = 0;
    };
}
