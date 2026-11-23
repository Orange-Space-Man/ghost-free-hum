#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/enemies/enemymanager.h"
#include "game/events/runtimeflags.h"

#include <functional>

namespace enemystrategy {
    enum class ConditionKind {
        Immediate,
        RoomEmpty,
        RoomCleared,
        PlayerInRoom,
        NamedAbsent,
        OccupiedUnclearedRoom,
        VisibleFlag,
        FlagOn,
        FlagOff,
        CandleAllOn
    };

    class Condition final : public Strategy {
    public:
        Condition(ConditionKind kind, enemies::Runtime& runtime, const gameflags::State& flags);
        void behavior() override;
        void initializeBehavior() override;
        void bindCandleCount(std::function<unsigned(unsigned)> callback);

    private:
        ConditionKind kind;
        enemies::Runtime& runtime;
        const gameflags::State& flags;
        std::int32_t flag;
        std::function<unsigned(unsigned)> candleCount;
    };
}
