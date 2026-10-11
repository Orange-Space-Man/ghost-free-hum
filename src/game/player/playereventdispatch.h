#pragma once

#include "game/player/playereventpayload.h"

namespace playeractor { class Player; }
namespace enemies { class Manager; }
namespace actors { class Registry; }

namespace playerevents {
    unsigned dispatch(playeractor::Player& player, unsigned message, const Payload* payload = nullptr);

    class Dispatcher {
    public:
        Dispatcher(enemies::Manager& manager, actors::Registry& actors, playeractor::Player* alternate = nullptr);
        unsigned send(std::int32_t player, unsigned message, const Payload* payload = nullptr) const;

    private:
        enemies::Manager& manager;
        actors::Registry& actors;
        playeractor::Player* alternate;
    };
}
