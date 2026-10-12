#pragma once

#include "game/player/playerdoorapproach.h"
#include "game/actors/actormodelcatalog.h"
#include "game/actors/actormodelresources.h"
#include "game/enemies/enemymanager.h"
#include "game/world/doorstate.h"

namespace playerinteraction {
    struct DoorInput {
        Vector position{}, direction{0, 0, 1};
        float speed = 0, radius = 0;
        unsigned actorType = 0, reaction = 0, animation = 0;
        float previousFrame = 0, frame = 0;
        std::uint32_t room = 0;
        bool eligible = false, pressed = false, close = false, reverse = false, primary = false;
        bool animationEnded = false, animationLooped = false, contact = false;
    };

    struct DoorOutput {
        DoorMotion motion;
        std::uint32_t room = 0;
        const Vector* position = nullptr;
        bool blocked = false, instantTurn = false, reverseFacing = false;
        unsigned damage = 0;
    };

    struct DoorServices {
        doors::State& doors;
        doors::SavedFlags& savedDoors;
        const std::array<std::uint8_t, 32>& keys;
        const std::array<std::uint32_t, 256>& roomIds;
        enemies::Manager& enemies;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        bool pairingDisabled = false, keyPresentationActive = false;
        std::function<void(bool, bool, std::uint8_t)> presentKey;
    };

    class DoorInteraction {
    public:
        explicit DoorInteraction(const std::vector<std::uint8_t>& executable);
        bool select(playercomponents::DoorState& state, const DoorInput& input, DoorServices& services) const;
        bool run(playercomponents::DoorState& state, playeractions::Actions& actions,
            const DoorInput& input, DoorOutput& output, DoorServices& services) const;
        static bool contains(const doors::CollisionBox& box, const Vector& position, float padding);

    private:
        void path(const playercomponents::DoorState& state, const playeractions::Actions& actions,
            const DoorInput& input, DoorOutput& output, const DoorServices& services) const;
        void open(playercomponents::DoorState& state, playeractions::Actions& actions,
            const DoorInput& input, DoorOutput& output, DoorServices& services) const;
        void arrive(playercomponents::DoorState& state, playeractions::Actions& actions,
            const DoorInput& input, DoorOutput& output, DoorServices& services) const;
        DoorApproach approach;
        playeractions::Actions fallback;
        float padding, facingThreshold;
        std::array<unsigned, 3> fakeTypes;
        Vector fakeOrigin, fakeSize;
    };
}
