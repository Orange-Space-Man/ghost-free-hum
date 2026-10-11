#include "game/events/eventconditions.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    std::int16_t signedHalf(std::uint32_t value) {
        const int low = static_cast<int>(value & 0xffff);
        return static_cast<std::int16_t>(low < 0x8000 ? low : low - 0x10000);
    }

    std::int32_t signedWord(std::uint32_t value) {
        std::int32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
}

void gameevent::ActivationState::reset() {
    std::fill_n(active.begin(), 109, std::uint8_t{0});
}

int gameevent::ActivationState::value(std::int16_t eventNumber) const {
    if (eventNumber > 109) return 0;
    if (eventNumber < 0) throw std::out_of_range("Negative event number.");
    const int result = active[eventNumber];
    return result < 128 ? result : result - 256;
}

void gameevent::ActivationState::configure(ConditionState& state, const Conditions& conditions,
    std::uint8_t completedCount, const gameflags::State& flags) {
    state.loadLimit = signedHalf(conditions.loadLimit);
    state.appearFlag = signedHalf(conditions.appearFlag);
    state.disappearFlag = signedHalf(conditions.disappearFlag);
    state.startTime = signedHalf(conditions.startTime);
    state.endTime = signedHalf(conditions.endTime);
    state.characterType = conditions.characterType;
    state.parameter = conditions.parameter;
    if (state.startTime != 0 || state.endTime != 0) state.flags |= 0x80;
    if (conditions.loadLimit == 0) state.flags |= 4;
    else if (state.loadLimit <= completedCount) state.flags |= 0x40;
    if (flags.value(static_cast<std::uint8_t>(state.disappearFlag))) state.flags |= 0x40;
    if (conditions.lock == 1) state.flags |= 0x100;
    if (conditions.playerStop == 1) state.flags |= 0x200;
    if ((state.flags & 0xc0) == 0 && state.appearFlag == 0) {
        if (state.eventNumber < 0 || state.eventNumber >= static_cast<int>(active.size()))
            throw std::out_of_range("Event number outside activation storage.");
        active[state.eventNumber] = 1;
    }
}

int gameevent::checkTime(const ConditionState& state, std::int32_t elapsedSeconds, std::int32_t leadSeconds) {
    if ((state.flags & 0x80) == 0) return 1;
    const auto start = signedWord(static_cast<std::uint32_t>(state.startTime) - static_cast<std::uint32_t>(leadSeconds));
    if (start <= elapsedSeconds && elapsedSeconds < state.endTime) return 1;
    if (state.endTime <= elapsedSeconds) return 2;
    return 0;
}

void gameevent::ActivationState::updateAppearance(const ConditionState& state, const gameflags::State& flags,
    std::int32_t elapsedSeconds) {
    if (state.flags & 0x40) return;
    if (!value(state.eventNumber) && state.appearFlag && flags.value(static_cast<std::uint8_t>(state.appearFlag)) &&
        checkTime(state, elapsedSeconds, 1) == 1) {
        active.at(state.eventNumber) = 1;
    }
}
