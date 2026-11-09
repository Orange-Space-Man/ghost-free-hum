#pragma once

#include "game/scene/gameplaymanagerservices.h"
#include "game/scene/gameplaycamera.h"
#include "game/scene/gameplaycameraprojection.h"
#include "game/world/roomresidency.h"
#include "game/world/roomfurnitureowner.h"
#include "game/world/roomvisibilitywalk.h"
#include "game/world/roommirror.h"

namespace gameplay {
    class ResourceOwner {
    public:
        ResourceOwner(const std::filesystem::path& path, gameentry::State& entry);
        ~ResourceOwner();
        ResourceOwner(const ResourceOwner&) = delete;
        ResourceOwner& operator=(const ResourceOwner&) = delete;
        ManagerServices& managers();
        const s3denvironment::Camera& camera() const;
        gameplaycamera::DrawState publishCamera();
        void updateCamera();
        void updateEvents(unsigned elapsedSeconds, std::uint32_t audioFrame);
        bool entranceActive() const;
        bool eventCameraActive() const;
        const roomlocation::Rooms& rooms() const;
        const std::vector<std::uint8_t>& executable() const;
        gameassets::Package& package();
        roomtraversal::State& visibility();
        std::vector<std::uint32_t>& roomFlags();
        const roomresidency::InitialRooms& residentRooms() const;
        roomfurniture::Owner& furniture();
        roommirror::Owner& mirror();
        std::vector<roomtraversal::Submission> prepareVisibility(const roomportal::Matrix& projection,
            const roomtraversal::Bounds& viewport, unsigned buffer = 0,
            const std::function<bool(const std::array<std::int32_t, 3>&)>& centerVisible = {});

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
