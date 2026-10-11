#pragma once

#include "game/enemies/enemyjewelstrategy.h"

namespace enemystrategy {
    class Gold final : public Jewel {
    public:
        Gold(ItemEnvironment& environment, const Factory& factory);

    private:
        void initializeItem() override;
        void groundedSound() override;
    };
}
