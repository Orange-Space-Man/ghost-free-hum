#pragma once

#include "game/enemies/enemyheartstrategy.h"

namespace enemystrategy {
    class HeartEscape final : public Heart {
    public:
        using Heart::Heart;

    private:
        void groundedSound() override;
    };
}
