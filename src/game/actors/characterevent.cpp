#include "game/actors/characterevent.h"

characterevent::Event::Event(unsigned message) : message(message) {}

std::uint32_t characterevent::Observer::receive(Event& event) {
    switch (event.message) {
    case 0: return 1;
    case 2: return enteredFlashlight(event);
    case 3: return captureBegin(event);
    case 4: return captureEnd(event);
    case 8: return replacement(event);
    case 9: return fishingBegin(event);
    case 10: return playerLeftRoom(event);
    case 11: return playerEnteredRoom(event);
    case 12: return collideWithPlayer(event);
    case 13: return sprayedWithFire(event);
    case 14: return sprayedWithIce(event);
    case 15: return sprayedWithWater(event);
    case 16: return hitWithFireBall(event);
    case 17: return hitWithIceBall(event);
    case 18: return hitWithWaterBall(event);
    case 19: return playerTapped(event);
    case 29: return resources(event);
    default: return 0;
    }
}

std::uint32_t characterevent::Observer::enteredFlashlight(Event&) { return 0; }
std::uint32_t characterevent::Observer::captureBegin(Event&) { return 0; }
std::uint32_t characterevent::Observer::captureEnd(Event&) { return 0; }
std::uint32_t characterevent::Observer::replacement(Event&) { return 0; }
std::uint32_t characterevent::Observer::collideWithPlayer(Event&) { return 0; }
std::uint32_t characterevent::Observer::sprayedWithFire(Event&) { return 0; }
std::uint32_t characterevent::Observer::sprayedWithIce(Event&) { return 0; }
std::uint32_t characterevent::Observer::sprayedWithWater(Event&) { return 0; }
std::uint32_t characterevent::Observer::hitWithFireBall(Event& event) { return sprayedWithFire(event); }
std::uint32_t characterevent::Observer::hitWithIceBall(Event& event) { return sprayedWithIce(event); }
std::uint32_t characterevent::Observer::hitWithWaterBall(Event& event) { return sprayedWithWater(event); }
std::uint32_t characterevent::Observer::fishingBegin(Event&) { return 0; }
std::uint32_t characterevent::Observer::playerLeftRoom(Event&) { return 0; }
std::uint32_t characterevent::Observer::playerEnteredRoom(Event&) { return 0; }
std::uint32_t characterevent::Observer::playerTapped(Event&) { return 0; }
std::uint32_t characterevent::Observer::resources(Event&) { return 0; }
