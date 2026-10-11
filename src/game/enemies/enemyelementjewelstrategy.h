#pragma once

#include "game/enemies/enemyjewelstrategy.h"

namespace enemystrategy {
    class ElementJewel final : public Jewel {
    public:
        ElementJewel(ItemEnvironment& environment, const Factory& factory);
        std::uint32_t captureEnd(characterevent::Event& event) override;
        bool awarded = false;

    private:
        void initializeItem() override;
        void onCollected() override;
        void groundedSound() override;
        bool infiniteLifetime() const override;
    };
}
