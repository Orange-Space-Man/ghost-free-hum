#pragma once

#include "game/player/playereventpayload.h"
#include "game/player/playermoneycollection.h"

namespace playeractor { class Player; }
namespace enemies { class Manager; }
namespace actors { class Registry; }

namespace playerevents {
    unsigned dispatch(playeractor::Player& player, unsigned message, const Payload* payload = nullptr,
        const CollectionSound& collectionSound = {});

    class Dispatcher {
    public:
        Dispatcher(enemies::Manager& manager, actors::Registry& actors, playeractor::Player* alternate = nullptr,
            CollectionSound collectionSound = {});
        unsigned send(std::int32_t player, unsigned message, const Payload* payload = nullptr) const;

    private:
        enemies::Manager& manager;
        actors::Registry& actors;
        playeractor::Player* alternate;
        CollectionSound collectionSound;
    };
}
