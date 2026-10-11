#pragma once

#include "game/enemies/enemyitemstrategy.h"
#include "game/actors/actortrailcontroller.h"

namespace enemystrategy {
    class Negative : public FiniteItem {
    public:
        Negative(ItemEnvironment& environment, const Factory& factory);
        actortrail::Controller trail;

    protected:
        virtual void groundedSound();
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
    };
}
