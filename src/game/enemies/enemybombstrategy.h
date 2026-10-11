#pragma once

#include "game/enemies/enemyitemstrategy.h"

namespace enemystrategy {
    class Bomb final : public FiniteItem {
    public:
        Bomb(ItemEnvironment& environment, const Factory& factory);
        ~Bomb() override;
        void begin() override;
        std::uint32_t captureEnd(characterevent::Event& event) override;
        std::uint32_t sprayedWithFire(characterevent::Event& event) override;
        std::uint32_t sprayedWithWater(characterevent::Event& event) override;
        std::uint32_t phase = 0;
        std::int16_t spin = 0;
        float baseScale = 1, radius = 0;
        actorcollision::Volume volume;
        std::array<particles::Emitter*, 2> emitters{};

    private:
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
        void releaseEmitters();
        void explode();
        void damagePlayer();
        void remove();
        void animation(const char* name);
        itemsuction::Vector fusePosition() const;
    };
}
