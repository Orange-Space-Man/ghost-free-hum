#pragma once

#include "game/enemies/enemystrategy.h"

namespace enemystrategy {
    class Replace final : public Strategy {
    public:
        void behavior() override;
        void initializeBehavior() override;
    };
}
