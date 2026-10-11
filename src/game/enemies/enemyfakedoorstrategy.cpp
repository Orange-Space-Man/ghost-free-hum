#include "game/enemies/enemyfakedoorstrategy.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

enemystrategy::FakeDoor::FakeDoor(const std::array<std::uint8_t, 4>& ambient) : ambient(ambient) {}

void enemystrategy::FakeDoor::behavior() {
    if (!owner) throw std::runtime_error("Original fake-door strategy has no actor owner.");
    auto& actor = dynamic_cast<actors::MoveActor&>(owner->actor());
    actor.setVelocity({});
    for (unsigned i = 0; i < 3; ++i) actor.fieldsE9[i] = ambient[i];
    actor.fieldEC = ambient[3];
}

std::uint32_t enemystrategy::FakeDoor::sprayedWithFire(characterevent::Event&) {
    throw std::runtime_error("Original fake-door fire reaction requires the later-area burn strategy.");
}
