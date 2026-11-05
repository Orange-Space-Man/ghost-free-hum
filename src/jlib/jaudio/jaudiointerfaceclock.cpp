#include "jlib/jaudio/jaudiointerfaceclock.h"

void jaudio::InterfaceClock::mixed() { ++dacFrame; }

bool jaudio::InterfaceClock::update(const std::function<void()>& work) {
    if (dacFrame.load() == previousDacFrame) return false;
    if (work) work();
    ++interfaceFrame;
    previousDacFrame = dacFrame.load();
    return true;
}

std::uint32_t jaudio::InterfaceClock::frame() const { return interfaceFrame; }

void jaudio::InterfaceClock::reset() {
    dacFrame = 0;
    previousDacFrame = interfaceFrame = 0;
}
