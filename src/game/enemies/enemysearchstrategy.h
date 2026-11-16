#pragma once

#include "game/enemies/enemyghostmovement.h"

namespace enemystrategy {
    struct SearchServices {
        RandomMovementServices movement;
        std::function<actors::MoveActor&(unsigned)> player;
    };

    class NearSearch final : public Strategy {
    public:
        explicit NearSearch(SearchServices services, float baseRadius = 200);
        void initializeBehavior() override;
        void behavior() override;

    private:
        SearchServices services;
        actors::MoveActor* tracked = nullptr;
        float radius = 0, floatingHeight = 0, direction = 1;
        float height() const;
        void vertical(std::array<float, 3>& velocity) const;
        void turn();
    };
}
