#pragma once

#include "game/actors/actorregistry.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"
#include "game/events/runtimeflags.h"

namespace enemies {
    class ManagerRuntime final : public Runtime {
    public:
        ManagerRuntime(const enemystrategy::Catalog& strategies, characterowner::Runtime& owners,
            actors::Registry& actors, const roomlocation::Rooms& rooms, const roomtraversal::State& visibility,
            const std::vector<std::uint32_t>& roomFlags, const gameflags::State& flags,
            const unsigned& mapMode, std::uint32_t& randomState);
        void bind(Manager& manager);
        std::unique_ptr<Owner> constructOwner(unsigned constructor, unsigned type) override;
        void resetActorIndex() override;
        Actor& constructActor(unsigned type, unsigned capacity) override;
        Owner& absentOwner() override;
        unsigned resolveRoom(const std::array<float, 3>& position) override;
        bool roomVisible(unsigned room, unsigned buffer) override;
        bool doorOpened(unsigned player, unsigned door) override;
        bool typeCaptured(unsigned player, unsigned type) override;
        bool roomCleared(unsigned roomGroup) override;
        std::uint32_t random() override;
        std::array<float, 3> playerPosition(unsigned player) override;
        unsigned playerRoom(unsigned player) override;
        bool playerSpecialState(unsigned player) override;

    private:
        const enemystrategy::Catalog& strategies;
        characterowner::Runtime& owners;
        actors::Registry& actors;
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        const std::vector<std::uint32_t>& roomFlags;
        const gameflags::State& flags;
        const unsigned& mapMode;
        std::uint32_t& randomState;
        characterowner::Owner sentinel;
        Manager* manager = nullptr;
        const playeractor::View& player(unsigned index) const;
    };
}
