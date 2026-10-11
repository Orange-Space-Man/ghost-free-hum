#pragma once

#include "game/enemies/enemyitemstrategy.h"

namespace enemystrategy {
    class Jewel : public FiniteItem {
    public:
        Jewel(ItemEnvironment& environment, const Factory& factory);
        float angle = 0, omega = 0;

    protected:
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
        virtual void groundedSound();
        virtual bool infiniteLifetime() const;
    };
}
