#pragma once

#include "game/player/playereventpayload.h"
#include "game/player/playermoneycollection.h"

namespace playeractor { class Player; }
namespace enemies { class Manager; }
namespace actors { class Registry; }

namespace playerevents {
    struct SuctionEnvironment;
    unsigned dispatch(playeractor::Player& player, unsigned message, const Payload* payload = nullptr,
        const CollectionSound& collectionSound = {}, const SuctionEnvironment* suction = nullptr);

    class Dispatcher {
    public:
        Dispatcher(enemies::Manager& manager, actors::Registry& actors, playeractor::Player* alternate = nullptr,
            CollectionSound collectionSound = {}, const SuctionEnvironment* suction = nullptr);
        playeractor::Player* player(std::int32_t index) const;
        unsigned send(std::int32_t player, unsigned message, const Payload* payload = nullptr) const;
        void bindDamage(std::function<bool(playeractor::Player&, const Payload&, bool)> handler);

    private:
        enemies::Manager& manager;
        actors::Registry& actors;
        playeractor::Player* alternate;
        CollectionSound collectionSound;
        const SuctionEnvironment* suction;
        std::function<bool(playeractor::Player&, const Payload&, bool)> damage;
    };
}
