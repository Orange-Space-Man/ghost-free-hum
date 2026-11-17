#pragma once

#include "game/enemies/enemyconditionstrategy.h"
#include "game/events/missionresources.h"

#include <memory>
#include <functional>

namespace actormodel { class InitialPose; }
namespace ghostparent { struct Environment; }
namespace particles { class EffectFactory; }

namespace enemystrategy {
    struct ItemEnvironment;
    struct TrainingEnvironment;
    struct BarrierEnvironment;
    struct FireGeneratorEnvironment;
    class Catalog;
    class Factory {
    public:
        Factory(enemies::Runtime& runtime, gameflags::State& flags, const mission::Resources& resources);
        std::unique_ptr<Strategy> construct(std::uint32_t constructor) const;
        void bindItems(ItemEnvironment* environment);
        void bindTraining(TrainingEnvironment& environment);
        void bindPose(const actormodel::InitialPose& pose);
        void bindGhostParent(ghostparent::Environment& environment);
        void bindRoomLight(std::function<void(unsigned)> callback);
        void bindGlobalDoorBarrierClear(std::function<void()> callback);
        void clearGlobalDoorBarrier() const;
        void bindKinopioEffects(particles::EffectFactory& effects);
        void lightRoom(unsigned room) const;
        void bindPlacement(const Catalog& catalog, BarrierEnvironment& barriers, FireGeneratorEnvironment& fire,
            const std::array<std::uint8_t, 4>& ambient);
        std::uint32_t random() const;

    private:
        enemies::Runtime& runtime;
        gameflags::State& flags;
        const mission::Resources& resources;
        ItemEnvironment* items = nullptr;
        TrainingEnvironment* training = nullptr;
        const Catalog* catalog = nullptr;
        BarrierEnvironment* barriers = nullptr;
        FireGeneratorEnvironment* fire = nullptr;
        const std::array<std::uint8_t, 4>* ambient = nullptr;
        const actormodel::InitialPose* pose = nullptr;
        ghostparent::Environment* ghost = nullptr;
        std::function<void(unsigned)> roomLight;
        std::function<void()> globalDoorBarrierClear;
        particles::EffectFactory* kinopioEffects = nullptr;
    };
}
