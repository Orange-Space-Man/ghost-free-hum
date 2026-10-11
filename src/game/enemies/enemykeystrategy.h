#pragma once

#include "game/enemies/enemyitemstrategy.h"

namespace enemystrategy {
    class Key final : public FiniteItem {
    public:
        Key(ItemEnvironment& environment, const Factory& factory);
        std::uint32_t captureEnd(characterevent::Event& event) override;
        bool awarded = false;

    private:
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
    };
}
