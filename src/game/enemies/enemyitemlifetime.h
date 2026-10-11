#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/scene/gameparameters.h"

namespace enemystrategy {
    class ItemLifetime {
    public:
        ItemLifetime();
        void initialize();
        void update(enemies::Owner& owner, bool infinite, bool collected, std::uint32_t& flags, bool& expired);
        gameparameters::Parameters parameters;
        std::uint32_t remaining = 0, blink = 0;
    };
}
