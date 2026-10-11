#include "game/enemies/enemyheartstrategy.h"
#include "game/player/playerliferecovery.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void load(gameparameters::Parameters& parameters, const gamearchive::Archive& archive, const std::string& path) {
        const int entry = archive.findPathResource(path);
        if (entry < 0) return;
        std::string error;
        if (!parameters.load(archive.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
    }
}

enemystrategy::Heart::Heart(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {
    parameters.add("mFirstOmega", 4, 0x3f800000);
    parameters.add("mOmegaDecay", 4, 0x3f7db22d);
    parameters.add("mRadius", 4, 0x41200000);
    parameters.add("mHeight", 4, 0x41a00000);
    parameters.addVector("mTrans", {0, 0xc1200000, 0});
}

void enemystrategy::Heart::begin() {
    const auto type = actor().type();
    if (type == 0xce) healing = 50;
    else if (type == 0xcd) {
        if (!environment.mansionMode) throw std::runtime_error("Original heart mansion mode is not bound.");
        const auto value = player().view.fieldsED4[0];
        const unsigned threshold = *environment.mansionMode == 0 ? 200 : 400;
        healing = static_cast<std::int32_t>(value) >= 0 && value >= threshold ? 20 : 10;
    }
}

void enemystrategy::Heart::initializeItem() {
    flags = (flags & ~2u) | 4;
    load(parameters, environment.common, "/param/th/EnHeartItem_" + environment.types.name(actor().type()) + ".prm");
    load(physics.parameters, environment.common, "/param/th/EnThrowUpItemHeart.prm");
    load(lifetime.parameters, environment.common, "/param/th/EnFiniteItemHeart.prm");
    const auto& values = parameters.entries();
    volume.field8 = 1;
    volume.secondary = actorcollision::Record{values[2].bits, values[3].bits,
        values[4].vectorBits[0], values[4].vectorBits[1], values[4].vectorBits[2]};
    actor().field788 = &volume;
}

void enemystrategy::Heart::beginActive() {
    angle = 0;
    omega = number(parameters.entries()[0].bits);
    beginFinite();
    environment.sounds.play(0x1875, actor().position, 0);
}

void enemystrategy::Heart::updateActive() {
    physics.update();
    auto& object = actor();
    if (physics.state == 2) {
        groundedSound();
        ++groundedFrames;
    }
    const float rotation = number(0x4622f983) * angle;
    const auto converted = !std::isfinite(rotation) || double(rotation) >= 2147483648.0 || rotation < -2147483648.0f ?
        std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(rotation);
    object.rotation[2] = object.previousRotation[0] = static_cast<std::int16_t>(converted);
    angle += omega;
    omega *= number(parameters.entries()[1].bits);
    constexpr double cycle = 6.28318530717958647692;
    if (double(angle) > cycle) angle = static_cast<float>(double(angle) - cycle);
    updateFinite(false);
}

void enemystrategy::Heart::onCollected() {
    angle = omega = 0;
    if (!healing) throw std::runtime_error("Original heart healing storage has not been initialized by its owner.");
    if (!environment.life) throw std::runtime_error("Original heart life services are not bound.");
    playerevents::recoverLife(player(), *healing, *environment.life);
}

void enemystrategy::Heart::groundedSound() {
    if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x1876, actor().position, 0);
}
