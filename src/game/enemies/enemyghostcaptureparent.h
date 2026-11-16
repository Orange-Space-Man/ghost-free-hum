#pragma once

#include "game/enemies/enemystrategydecorator.h"
#include "game/actors/characterevent.h"
#include "game/enemies/enemyghostdisappearance.h"
#include <memory>

namespace enemystrategy {
    class GhostCaptureParent : public Decorator, public characterevent::Observer {
    public:
        std::uint32_t fishingBegin(characterevent::Event& event) final;
        std::uint32_t captureBegin(characterevent::Event& event) final;
        void initializeBehavior() final;
        void behavior() final;
        bool query(enemies::Owner& owner) final;
        void completeDisappearance();

        bool captureLatch = false;
        std::uint32_t lightState = 0;

    protected:
        virtual std::unique_ptr<Strategy> createCapture() = 0;
        virtual void notifyAttackFishing(characterevent::Event& event) = 0;
        virtual void initializeGhostState() = 0;
        virtual void updateGhostState() = 0;
        virtual void updateKnockback() = 0;
        virtual GhostDisappearance& disappearance() = 0;
        virtual void finishSecondary() = 0;
        virtual void clearElement() = 0;

    private:
        std::unique_ptr<Strategy> capture;
    };
}
