#include "game/enemies/enemynegativestrategy.h"
#include "game/player/playershrinkevent.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void load(gameparameters::Parameters& parameters, const gamearchive::Archive& archive, const char* path) {
        const int entry = archive.findPathResource(path);
        if (entry < 0) return;
        std::string error;
        if (!parameters.load(archive.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
    }
}

enemystrategy::Negative::Negative(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {
    auto& parameters = environment.negativeParameters;
    if (parameters.entries().empty()) {
        parameters.add("mScaleRatio", 4, 0x3e99999a);
        parameters.add("mScaleFrame", 4, 300);
        parameters.add("mTailColor", 4, 0xff00c8ff);
        parameters.add("mTailLife", 4, 30);
    }
}

void enemystrategy::Negative::initializeItem() {
    flags = (flags | 4) & ~2u;
    load(environment.negativeParameters, environment.common, "/param/th/EnNegativeItem.prm");
    load(physics.parameters, environment.common, "/param/th/EnThrowUpMkinoko.prm");
    load(lifetime.parameters, environment.common, "/param/th/EnFiniteKinoko.prm");
    const auto& values = environment.negativeParameters.entries();
    trail.initialize(&actor(), {0, 0}, {{{30, 30, 0}, {-30, -30, 0}}}, 2,
        values[2].bits, static_cast<std::uint8_t>(values[3].bits), 0, 0);
}

void enemystrategy::Negative::beginActive() {
    bind(physics);
    physics.setNextState(0);
    groundedFrames = 0;
    environment.sounds.play(0x1898, actor().position, 0);
    suction.initialize(actor(), 0, environment.durationBase);
    lifetime.initialize();
    trail.reset();
}

void enemystrategy::Negative::updateActive() {
    physics.update();
    if (physics.state == 2) {
        groundedSound();
        ++groundedFrames;
    }
    updateFinite(false);
    const auto room = environment.suction.rooms.resolve(actor().position, actor().fieldB4);
    if (environment.suction.visibility.record(room, 0).flags & 1) trail.update(environment.trails);
}

void enemystrategy::Negative::onCollected() {
    if (player().view.fieldF15) return;
    if (!environment.shrink) throw std::runtime_error("Original mushroom shrink services are not bound.");
    const auto& values = environment.negativeParameters.entries();
    float ratio;
    std::memcpy(&ratio, &values[0].bits, sizeof(ratio));
    playerevents::shrink(player(), ratio, values[1].bits, *environment.shrink);
    environment.suction.effects.create(0x46, actor().position, {});
}

void enemystrategy::Negative::groundedSound() {
    if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x1876, actor().position, 0);
}
