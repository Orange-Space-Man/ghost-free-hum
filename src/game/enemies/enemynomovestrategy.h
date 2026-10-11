#pragma once

#include "game/enemies/enemystrategy.h"

namespace enemystrategy {
    class NoMove final : public Strategy {
    public:
        void behavior() override;
    };
}
