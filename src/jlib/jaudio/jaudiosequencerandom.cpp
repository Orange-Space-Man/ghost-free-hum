#include "jlib/jaudio/jaudiosequencerandom.h"

#include <chrono>
#include <mutex>

std::uint32_t jaudio::SequenceRandom::next(std::uint32_t tick) {
    state[1] = static_cast<std::uint8_t>(state[1] + (state[6] >> 1) + state[7] - state[0]);
    state[2] = static_cast<std::uint8_t>(state[2] + (state[7] >> 2) + state[1] - state[4]);
    state[3] = static_cast<std::uint8_t>(state[3] + state[2] + state[5] - (state[4] >> 1));
    state[0] = static_cast<std::uint8_t>(state[0] - state[3] - state[6] - (state[5] >> 2));
    for (unsigned index = 0; index < 4; ++index) state[index + 4] = static_cast<std::uint8_t>(state[index] + (tick >> (index * 2)));
    constexpr std::array<std::array<unsigned, 4>, 4> order{{{0, 3, 2, 1}, {1, 2, 3, 0}, {2, 0, 1, 3}, {3, 1, 0, 2}}};
    std::uint32_t value = 0;
    for (const auto index : order[tick & 3]) value = (value << 8) | state[index];
    if (tick & 8) ++value;
    if (tick & 16) value = 0u - value;
    return value;
}

std::uint32_t jaudio::sequenceRandom() {
    static SequenceRandom generator;
    static std::mutex mutex;
    static const auto origin = std::chrono::steady_clock::now();
    const std::lock_guard<std::mutex> lock(mutex);
    const auto elapsed = std::chrono::steady_clock::now() - origin;
    using TimerDuration = std::chrono::duration<std::uint64_t, std::ratio<1, 40500000>>;
    return generator.next(static_cast<std::uint32_t>(std::chrono::duration_cast<TimerDuration>(elapsed).count()));
}
