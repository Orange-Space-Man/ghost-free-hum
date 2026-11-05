#pragma once

#include "playersheetframe.h"
#include "game/player/playereventdispatch.h"

#include <optional>

namespace playersheets {
    class PlayerBinding {
    public:
        PlayerBinding(playerevents::Dispatcher& dispatcher, int index);
        const SuctionOwner& owner();
        bool holding() const;
        bool pulling() const;
        bool request(Grid& sheet, const playerscanner::Vector& center, float range, float elasticity);
        void release(Grid& sheet);
        void complete(const playerevents::Payload& payload);

    private:
        playeractor::Player& player() const;
        playerevents::Dispatcher& dispatcher;
        int index;
        std::optional<SuctionOwner> binding;
    };
}
