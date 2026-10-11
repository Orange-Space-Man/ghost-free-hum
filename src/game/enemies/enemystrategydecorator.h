#pragma once

#include "game/enemies/enemystrategy.h"

namespace enemystrategy {
    class Decorator : public Strategy {
    public:
        void update() override;
        Strategy* active = nullptr;
        std::uint16_t result = 0x100;
    };
}
