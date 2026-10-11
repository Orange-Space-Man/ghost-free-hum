#pragma once

#include "game/enemies/enemystrategyfactory.h"
#include "game/enemies/enemystrategycatalog.h"
#include "game/actors/characterevent.h"

namespace enemystrategy {
    class RoomObserver final : public Strategy, public characterevent::Observer {
    public:
        RoomObserver(const Factory& factory, const Catalog& catalog);
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t playerLeftRoom(characterevent::Event& event) override;

    private:
        const Factory& factory;
        const Catalog& catalog;
        std::unique_ptr<Strategy> condition;
        unsigned action = 0;
    };
}
