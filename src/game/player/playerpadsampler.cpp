#include "resources/dolresources.h"
#include "game/player/playerpadsampler.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void clampStick(std::int8_t& x, std::int8_t& y, int maximum, int diagonal, int minimum) {
        const int signX = x < 0 ? -1 : 1, signY = y < 0 ? -1 : 1;
        int ax = std::max(std::abs(int(x)) - minimum, 0), ay = std::max(std::abs(int(y)) - minimum, 0);
        const int limit = diagonal * maximum;
        const int distance = diagonal * std::max(ax, ay) + std::min(ax, ay) * (maximum - diagonal);
        if (distance > limit) {
            ax = ax * limit / distance;
            ay = ay * limit / distance;
        }
        x = static_cast<std::int8_t>(signX * ax);
        y = static_cast<std::int8_t>(signY * ay);
    }
}

playerinput::PadSampler::PadSampler(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x804d7ef0, 8, bytes, error)) throw std::runtime_error(error);
    std::copy(bytes.begin(), bytes.end(), clamp.begin());
    if (!dolresources::read(executable, 0x804dd6c8, 16, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < thresholds.size(); ++i) {
        const auto offset = i * 4;
        const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
        std::memcpy(&thresholds[i], &bits, sizeof(float));
    }
    if (!dolresources::read(executable, 0x804d7e58, 4, bytes, error)) throw std::runtime_error(error);
    normalize = bytes == std::vector<std::uint8_t>{0, 0, 0, 1};
}

unsigned playerinput::PadSampler::stick(std::int8_t x, std::int8_t y, float divisor,
    std::array<float, 2>& result, float& magnitude) const {
    result = {float(x) / divisor, float(y) / divisor};
    magnitude = std::sqrt(result[0] * result[0] + result[1] * result[1]);
    if (magnitude > 1) {
        if (normalize) {
            result[0] /= magnitude;
            result[1] /= magnitude;
        }
        magnitude = 1;
    }
    unsigned buttons = 0;
    for (unsigned i = 0; i < result.size(); ++i) {
        const float value = result[i];
        if (value <= thresholds[0] || thresholds[1] <= value) {
            if (value <= thresholds[2]) buttons |= 1 << (i * 2);
            else if (thresholds[3] <= value) buttons |= 2 << (i * 2);
        }
    }
    return buttons;
}

void playerinput::PadSampler::update(Pad& pad, PadStatus status) const {
    if (status.error != 0 && status.error != -1) {
        pad.pressed = pad.released = 0;
        return;
    }
    if (status.error == -1) status = {};
    else {
        clampStick(status.stickX, status.stickY, clamp[3], clamp[4], clamp[2]);
        clampStick(status.substickX, status.substickY, clamp[6], clamp[7], clamp[5]);
        const auto trigger = [&](unsigned value) {
            return static_cast<std::uint8_t>(value <= clamp[0] ? 0 : std::min(value, unsigned(clamp[1])) - clamp[0]);
        };
        status.triggerLeft = trigger(status.triggerLeft);
        status.triggerRight = trigger(status.triggerRight);
    }
    const auto mainButtons = stick(status.stickX, status.stickY, 54, pad.stick, pad.stickMagnitude);
    const auto subButtons = stick(status.substickX, status.substickY, 42, pad.substick, pad.substickMagnitude);
    const auto held = status.buttons | (mainButtons << 24) | (subButtons << 16);
    pad.pressed = held & (held ^ pad.held);
    pad.released = pad.held & (held ^ pad.held);
    pad.held = held;
    pad.analogA = status.analogA;
    pad.analogB = status.analogB;
    pad.triggerLeft = status.triggerLeft;
    pad.triggerRight = status.triggerRight;
}
