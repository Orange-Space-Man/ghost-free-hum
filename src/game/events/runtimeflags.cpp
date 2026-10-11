#include "game/events/runtimeflags.h"

void gameflags::State::clear() {
    flags.fill(0);
}

bool gameflags::State::value(std::uint8_t flag) const {
    return ((flags[flag >> 3] >> (flag & 7)) & 1) != 0;
}

void gameflags::State::setOn(std::uint8_t flag) {
    flags[flag >> 3] |= static_cast<std::uint8_t>(1U << (flag & 7));
}

void gameflags::State::setOff(std::uint8_t flag) {
    flags[flag >> 3] &= static_cast<std::uint8_t>(~(1U << (flag & 7)));
}

void gameflags::State::restore(const std::array<std::uint8_t, 11>& saved) {
    for (std::uint8_t flag = 0; flag < 85; ++flag) {
        if ((saved[flag >> 3] >> (flag & 7)) & 1) setOn(flag);
        else setOff(flag);
    }
}

void gameflags::State::store(std::array<std::uint8_t, 11>& saved) const {
    for (unsigned i = 0; i < 10; ++i) saved[i] = flags[i];
    saved[10] = static_cast<std::uint8_t>((saved[10] & 0xe0) | (flags[10] & 0x1f));
}
