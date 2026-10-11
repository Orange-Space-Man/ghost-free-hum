#include "platform/windows/gameframepacing.h"

#include <cmath>
#include <stdexcept>

bool gameframepacing::Clock::ready(double seconds) {
    if (!std::isfinite(seconds)) throw std::runtime_error("Invalid presentation time.");
    if (!started) {
        next = seconds;
        started = true;
    }
    if (seconds < next) return false;
    constexpr double period = 1.0 / presentationFramesPerSecond;
    next += (std::floor((seconds - next) / period) + 1) * period;
    return true;
}
