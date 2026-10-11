#pragma once

#include "game/enemies/enemynegativestrategy.h"

namespace enemystrategy {
    class NegativeRandom final : public Negative {
    public:
        using Negative::Negative;

    private:
        void groundedSound() override;
    };
}
