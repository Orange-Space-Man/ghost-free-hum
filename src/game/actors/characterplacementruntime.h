#pragma once

#include "game/events/jmpentryactivation.h"
#include "game/enemies/enemymanager.h"
#include "game/world/roomdependencies.h"

namespace characterplacement {
    class Runtime final : public jmp::ActivationRuntime {
    public:
        Runtime(enemies::Manager& manager, enemies::Runtime& actors, const roomdependencies::Catalog& dependencies);
        unsigned resolveRoom(const std::array<float, 3>& position) override;
        bool roomVisible(unsigned room) override;

    private:
        enemies::Manager& manager;
        enemies::Runtime& actors;
        const roomdependencies::Catalog& dependencies;
    };
}
