#pragma once

#include "game/events/runtimeflags.h"

namespace gameevent {
    struct Conditions {
        std::uint32_t startTime = 0;
        std::uint32_t endTime = 0;
        std::uint8_t lock = 0;
        std::uint32_t loadLimit = 0;
        std::uint32_t appearFlag = 0;
        std::uint32_t disappearFlag = 0;
        std::uint32_t playerStop = 0;
        std::uint32_t parameter = 0;
        std::uint32_t characterType = 0;
    };

    struct ConditionState {
        std::uint32_t flags = 0;
        std::int16_t eventNumber = 0;
        std::int32_t startTime = 0;
        std::int32_t endTime = 0;
        std::int16_t loadLimit = 0;
        std::int16_t appearFlag = 0;
        std::int16_t disappearFlag = 0;
        std::uint32_t parameter = 0;
        std::uint32_t characterType = 0;
    };

    class ActivationState {
    public:
        void reset();
        int value(std::int16_t eventNumber) const;
        void configure(ConditionState& state, const Conditions& conditions,
            std::uint8_t completedCount, const gameflags::State& flags);
        void updateAppearance(const ConditionState& state, const gameflags::State& flags, std::int32_t elapsedSeconds);

    private:
        std::array<std::uint8_t, 110> active{};
    };

    int checkTime(const ConditionState& state, std::int32_t elapsedSeconds, std::int32_t leadSeconds);
}
