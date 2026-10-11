#include "game/enemies/enemyelementjewelstrategy.h"

#include <cmath>

enemystrategy::ElementJewel::ElementJewel(ItemEnvironment& environment, const Factory& factory) : Jewel(environment, factory) {}

void enemystrategy::ElementJewel::initializeItem() {
    flags |= 0x16;
    actor().selectAnimation("NORMAL");
}

void enemystrategy::ElementJewel::onCollected() {
    awarded = true;
    Jewel::onCollected();
}

void enemystrategy::ElementJewel::groundedSound() {
    if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x18bc, actor().position, 0);
}

bool enemystrategy::ElementJewel::infiniteLifetime() const { return true; }

std::uint32_t enemystrategy::ElementJewel::captureEnd(characterevent::Event& event) {
    FiniteItem::captureEnd(event);
    if (awarded) {
        const auto type = actor().type();
        environment.suction.flags.setOn(type == 0x75 ? 0x2b : type == 0x76 ? 0x2d : type == 0x77 ? 0x2c : 0);
    }
    return 1;
}
