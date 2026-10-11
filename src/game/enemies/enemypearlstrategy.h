#pragma once

#include "game/enemies/enemyjewelstrategy.h"

namespace enemystrategy {
    class Pearl final : public Jewel {
    public:
        Pearl(ItemEnvironment& environment, const Factory& factory);
        void bindSource(unsigned type);
        std::int32_t sourceIndex = -1;

    private:
        void initializeItem() override;
        void prepareCollection(playerevents::ItemRequest& request) override;
    };
}
