#pragma once

#include "game/enemies/enemyconditionstrategy.h"
#include "game/events/missionresources.h"

#include <memory>

namespace actormodel { class InitialPose; }

namespace enemystrategy {
    struct ItemEnvironment;
    struct BarrierEnvironment;
    struct FireGeneratorEnvironment;
    class Catalog;
    class Factory {
    public:
        Factory(enemies::Runtime& runtime, const gameflags::State& flags, const mission::Resources& resources);
        std::unique_ptr<Strategy> construct(std::uint32_t constructor) const;
        void bindItems(ItemEnvironment* environment);
        void bindPose(const actormodel::InitialPose& pose);
        void bindPlacement(const Catalog& catalog, BarrierEnvironment& barriers, FireGeneratorEnvironment& fire,
            const std::array<std::uint8_t, 4>& ambient);
        std::uint32_t random() const;

    private:
        enemies::Runtime& runtime;
        const gameflags::State& flags;
        const mission::Resources& resources;
        ItemEnvironment* items = nullptr;
        const Catalog* catalog = nullptr;
        BarrierEnvironment* barriers = nullptr;
        FireGeneratorEnvironment* fire = nullptr;
        const std::array<std::uint8_t, 4>* ambient = nullptr;
        const actormodel::InitialPose* pose = nullptr;
    };
}
