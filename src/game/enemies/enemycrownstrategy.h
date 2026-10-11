#pragma once

#include "game/enemies/enemyjewelstrategy.h"

namespace enemystrategy {
    class Crown final : public Jewel {
    public:
        Crown(ItemEnvironment& environment, const Factory& factory);

    private:
        void groundedSound() override;
        bool infiniteLifetime() const override;
    };
}
