#pragma once

#include "game/enemies/enemyitemstrategy.h"

namespace enemystrategy {
    class Heart : public FiniteItem {
    public:
        Heart(ItemEnvironment& environment, const Factory& factory);
        void begin() override;
        gameparameters::Parameters parameters;
        actorcollision::Volume volume;
        float angle = 0, omega = 0;
        std::optional<std::uint32_t> healing;

    protected:
        virtual void groundedSound();
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
    };
}
