#include "game/startup/scenetransition.h"
#include <algorithm>

void scenetransition::OutgoingFade::reset() { *this = {}; }
void scenetransition::OutgoingFade::begin() { ticks = 0; started = true; }
void scenetransition::OutgoingFade::update(unsigned viTicks) {
    if (started) ticks += std::min(viTicks, 30U - ticks);
}
bool scenetransition::OutgoingFade::active() const { return started; }
bool scenetransition::OutgoingFade::finished() const { return started && ticks == 30; }
unsigned scenetransition::OutgoingFade::alpha() const { return ticks * 255 / 30; }
