#pragma once

#include "game/player/playeractor.h"

namespace actors {
    class Registry {
    public:
        Registry(const std::vector<std::uint8_t>& executable, const Construction& construction,
            players::Manager& manager, players::Globals& sharedGlobals, gameplaycamera::CameraOwner& camera,
            playeractor::Globals& globals, const actorcollision::Catalog& collisions,
            const std::vector<std::uint32_t>& roomFlags, unsigned mapMode, bool flag3D);
        ~Registry();
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;
        MoveActor& create(unsigned type, unsigned capacity);
        void resetIndex();
        void destroyActors();
        void updatePoses(const std::function<void()>& captureUpdate,
            const std::function<void(MoveActor&)>& animationSound);
        void updateContacts(enemies::Manager& manager, const actorcollision::Manager& collision,
            const playerresources::RecordPool44& pool, const mapcollision::State& map,
            const actorcollision::Catalog& catalog, gameflags::State& flags);
        void updateCaptures(actorcapture::Motion& motion, const playermodels::VacuumPath* path,
            const actorcapture::Vector* suctionDirection, actormodel::Matrix& retainedTransform,
            const actorcapture::Vector* originalScratch = nullptr, const float* originalRegister = nullptr,
            const bool* emptyCompletion = nullptr);
        MoveActor* actor(unsigned index) const;
        unsigned count() const;
        const std::array<MoveActor*, 128>& entries() const;

    private:
        const Construction& construction;
        std::array<MoveActor*, 128> objects{};
        std::array<std::unique_ptr<MoveActor>, 128> owned;
        playeractor::ViewEnvironment environment;
        playeractor::Construction playerConstruction;
        unsigned used = 0;
    };
}
