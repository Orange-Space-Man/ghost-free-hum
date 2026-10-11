#pragma once

#include "game/enemies/enemystrategy.h"

#include <array>

namespace enemystrategy {
    class GeneratorDoor final : public Strategy {
    public:
        void initializeBehavior() override;
        void behavior() override;

    private:
        std::array<float, 3> position;
    };
}
