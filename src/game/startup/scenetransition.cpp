#include "game/startup/scenetransition.h"
#include <algorithm>
#include <stdexcept>

void scenetransition::OutgoingFade::reset() { *this = {}; }
void scenetransition::OutgoingFade::begin() { ticks = 0; started = true; }
void scenetransition::OutgoingFade::update(unsigned viTicks) {
    if (started) ticks += std::min(viTicks, 30U - ticks);
}
bool scenetransition::OutgoingFade::active() const { return started; }
bool scenetransition::OutgoingFade::finished() const { return started && ticks == 30; }
unsigned scenetransition::OutgoingFade::alpha() const { return ticks * 255 / 30; }

void scenetransition::ParlorWipe::beginOutgoing(const std::array<std::uint8_t, 3>& retainedColor, unsigned length) {
    if (!length || length > 32767) throw std::invalid_argument("Scene fade requires a positive signed-short duration.");
    duration = length;
    color = retainedColor;
    phase = Phase::Outgoing;
    ticks = 0;
    delay = 0;
}

void scenetransition::ParlorWipe::beginIncoming() {
    phase = Phase::Incoming;
    ticks = duration;
    delay = 3;
}

bool scenetransition::ParlorWipe::update(unsigned viTicks) {
    if (delay != 0) {
        --delay;
        return false;
    }
    if (phase == Phase::Outgoing) {
        ticks += std::min(viTicks, duration - ticks);
        if (ticks == duration) {
            phase = Phase::Covered;
            return true;
        }
    } else if (phase == Phase::Incoming) {
        ticks -= std::min(viTicks, ticks);
        if (ticks == 0) {
            phase = Phase::Idle;
            return true;
        }
    }
    return false;
}

unsigned scenetransition::ParlorWipe::alpha() const { return ticks * 255 / duration; }
bool scenetransition::ParlorWipe::active() const { return phase != Phase::Idle; }

bool scenetransition::ParlorWipe::drawsCapturedFrame() const { return phase == Phase::Outgoing; }
std::array<std::uint8_t, 4> scenetransition::ParlorWipe::overlay() const {
    return {color[0], color[1], color[2], static_cast<std::uint8_t>(alpha())};
}
