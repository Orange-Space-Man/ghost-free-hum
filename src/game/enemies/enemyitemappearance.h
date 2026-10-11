#pragma once

#include "game/enemies/enemystrategy.h"

namespace enemystrategy {
    class ItemImmediateAppearance final : public Strategy {
    public:
        void behavior() override;
    };

    class ItemFadeAppearance final : public Strategy {
    public:
        void initializeBehavior() override;
        void behavior() override;
    };
}
