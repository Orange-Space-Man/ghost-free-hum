#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include <functional>

namespace ghostparent {
    struct BanaobaServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        std::function<float()> birth;
        std::function<void()> createBanana;
    };

    // EnAtkBanaoba, attack selector 0x65 (800DF040-800DF404).
    class BanaobaAttack final : public enemystrategy::Strategy {
    public:
        explicit BanaobaAttack(BanaobaServices services);
        void initializeBehavior() override;
        void behavior() override;

    private:
        BanaobaServices services;
    };
}
