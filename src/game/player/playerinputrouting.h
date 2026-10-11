#pragma once

#include "game/player/playeractor.h"
#include "game/player/playerinputdecoder.h"

#include <functional>

namespace playerinput {
    struct RouteServices {
        const gameplaycamera::CameraOwner& camera;
        bool inputAvailable;
        unsigned mapMode;
        std::function<void(playeractor::Player&, bool)> scannerTransition;
    };

    void route(playeractor::Player& player, const Decoder& decoder, const RouteServices& services);
}
