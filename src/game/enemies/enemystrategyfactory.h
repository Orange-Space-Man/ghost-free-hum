#pragma once

#include "game/enemies/enemyconditionstrategy.h"
#include "game/events/missionresources.h"

#include <memory>
#include <functional>
#include <string_view>

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
        void bindTreasure(const std::vector<std::uint32_t>& rooms,
            std::function<void(unsigned, const std::array<float, 3>&)> sound,
            std::function<unsigned(std::string_view, const std::array<float, 3>&)> createItem,
            std::function<void(const std::array<float, 3>&)> revealCamera,
            std::function<bool()> automaticallyOpenKeyChests,
            std::function<void(unsigned)> markOpened);
        void bindPose(const actormodel::InitialPose& pose);
        void bindGhostParent(ghostparent::Environment& environment);
        void bindRoomLight(std::function<void(unsigned)> callback);
        void bindGlobalDoorBarrierClear(std::function<void()> callback);
        void bindGlobalDoorBarrierSet(std::function<void()> callback);
        void bindDoorLockScare(std::function<void()> callback);
        void clearGlobalDoorBarrier() const;
        void setGlobalDoorBarrier() const;
        void scareForDoorLock() const;
        void setEventFlag(unsigned flag) const;
        void bindKinopioEffects(particles::EffectFactory& effects);
        void bindCandleCount(std::function<unsigned(unsigned)> callback);
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
        std::function<unsigned(unsigned)> candleCount;
        std::function<void()> globalDoorBarrierClear;
        std::function<void()> globalDoorBarrierSet;
        std::function<void()> doorLockScare;
        particles::EffectFactory* kinopioEffects = nullptr;
        const std::vector<std::uint32_t>* treasureRooms = nullptr;
        std::function<void(unsigned, const std::array<float, 3>&)> treasureSound;
        std::function<unsigned(std::string_view, const std::array<float, 3>&)> treasureItem;
        std::function<void(const std::array<float, 3>&)> treasureRevealCamera;
        std::function<bool()> automaticallyOpenKeyChests;
        std::function<void(unsigned)> treasureMarkOpened;
    };
}
