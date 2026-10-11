#include "game/input/controllerfeedbackpulse.h"

namespace {
    std::uint32_t unsignedInteger(double value) {
        if (!(value >= 0)) return 0;
        if (value >= 4294967296.0) return 0xffffffff;
        return static_cast<std::uint32_t>(value);
    }
}

void controllerfeedback::Pulses::play(double amplitude, std::uint32_t duration, std::uint32_t fade) {
    auto* selected = &slots[0];
    std::uint32_t longest = 0;
    for (auto& slot : slots) {
        if (!slot.active) {
            selected = &slot;
            break;
        }
        const auto remaining = slot.remaining + slot.fadeRemaining;
        if (remaining > longest) {
            longest = remaining;
            selected = &slot;
        }
    }
    selected->active = true;
    selected->amplitude = static_cast<float>(amplitude);
    selected->remaining = unsignedInteger(static_cast<float>(duration));
    if (duration && !selected->remaining) selected->remaining = 1;
    selected->fade = unsignedInteger(static_cast<float>(fade));
    if (fade && !selected->fade) selected->fade = 1;
    selected->fadeRemaining = selected->fade;
    selected->inverseFade = 1.0f / static_cast<float>(selected->fade);
}

int controllerfeedback::Pulses::update(std::uint32_t frame) {
    float maximum = 0;
    for (auto& slot : slots) {
        if (!slot.active) continue;
        float amplitude = 0;
        if (slot.remaining) amplitude = slot.amplitude;
        else if (slot.fadeRemaining) {
            const float ratio = static_cast<float>(slot.fadeRemaining) * slot.inverseFade;
            amplitude = (ratio * slot.amplitude) * ratio;
        }
        if (maximum < amplitude) maximum = amplitude;
        if (slot.remaining) {
            --slot.remaining;
            if (!slot.remaining && !slot.fadeRemaining) slot.active = false;
        }
        else if (slot.fadeRemaining && !--slot.fadeRemaining) slot.active = false;
    }
    if (maximum <= 0) return '/';
    const auto threshold = unsignedInteger(16.0f * maximum);
    const auto phase = frame & 15;
    const auto reversed = ((phase & 1) << 3) | ((phase & 2) << 1) | ((phase & 4) >> 1) | ((phase & 8) >> 3);
    return reversed < threshold ? '*' : '-';
}

void controllerfeedback::Pulses::reset() {
    slots = {};
}
