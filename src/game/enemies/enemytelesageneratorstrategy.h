#pragma once

#include "game/actors/charactergenerator.h"

namespace enemystrategy {
    class TelesaGenerator final : public Strategy {
    public:
        explicit TelesaGenerator(enemies::Runtime& runtime);
        void initializeBehavior() override;
        void behavior() override;

    private:
        charactergenerator::Generator& generator() const;
        enemies::Runtime& runtime;
        std::array<float, 3> position{};
        int appearanceFrame = 0;
    };
}
