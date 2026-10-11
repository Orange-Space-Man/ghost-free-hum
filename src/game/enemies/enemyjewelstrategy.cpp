#include "game/enemies/enemyjewelstrategy.h"
#include "game/enemies/enemygoldstrategy.h"
#include "game/enemies/enemycrownstrategy.h"

#include <cmath>
#include <limits>
#include <stdexcept>

enemystrategy::Jewel::Jewel(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {}

void enemystrategy::Jewel::initializeItem() {
    flags |= 6;
    actor().selectAnimation("NORMAL");
    if (actor().type() == 0xca) {
        const int entry = environment.common.findPathResource("/param/th/EnFiniteRdiamond.prm");
        if (entry >= 0) {
            std::string error;
            if (!lifetime.parameters.load(environment.common.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
        }
    }
}

void enemystrategy::Jewel::beginActive() {
    angle = 0;
    omega = 0.06f;
    beginFinite();
    if (actor().type() != 0xcc) environment.sounds.play(0x1875, actor().position, 0);
}

void enemystrategy::Jewel::groundedSound() {
    if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x1879, actor().position, 0);
}

bool enemystrategy::Jewel::infiniteLifetime() const { return false; }

void enemystrategy::Jewel::updateActive() {
    physics.update();
    if (physics.state == 2) {
        groundedSound();
        ++groundedFrames;
    }
    auto& object = actor();
    const float rotation = 10430.378f * angle;
    const auto converted = !std::isfinite(rotation) || double(rotation) >= 2147483648.0 || rotation < -2147483648.0f ?
        std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(rotation);
    object.rotation[2] = object.previousRotation[0] = static_cast<std::int16_t>(converted);
    angle += omega;
    constexpr double cycle = 6.28318530717958647692;
    if (double(angle) > cycle) angle = static_cast<float>(double(angle) - cycle);
    updateFinite(infiniteLifetime());
}

void enemystrategy::Jewel::onCollected() { angle = omega = 0; }


enemystrategy::Gold::Gold(ItemEnvironment& environment, const Factory& factory) : Jewel(environment, factory) {}

void enemystrategy::Gold::initializeItem() {
    Jewel::initializeItem();
    flags &= ~2u;
}

void enemystrategy::Gold::groundedSound() {
    if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x18cb, actor().position, 0);
}


enemystrategy::Crown::Crown(ItemEnvironment& environment, const Factory& factory) : Jewel(environment, factory) {}

void enemystrategy::Crown::groundedSound() {
    if (!actor().parent) Jewel::groundedSound();
}

bool enemystrategy::Crown::infiniteLifetime() const { return true; }
