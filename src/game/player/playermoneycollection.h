#pragma once

#include "game/player/playeractor.h"
#include "game/player/playereventpayload.h"

#include <functional>

namespace playerevents {
    using CollectionSound = std::function<void(unsigned, const std::array<float, 3>&)>;

    std::int32_t collectSuctionMoney(playeractor::Player& player, unsigned kind, unsigned amount,
        const CollectionSound& sound);
    unsigned collectMoney(playeractor::Player& player, unsigned message, const Payload& payload,
        const CollectionSound& sound);
}
