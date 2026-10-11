#include "game/enemies/enemykeystrategy.h"
#include "game/actors/characterowner.h"
#include "game/player/playerkeycollection.h"

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

    bool valid(const jmp::ToolDataRef& source) {
        return source.table && source.row >= 0;
    }
}

enemystrategy::Key::Key(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {
    auto& parameters = environment.keyParameters;
    if (parameters.entries().empty()) parameters.add("sIgnoreColFrame", 4, 0x41200000);
    load(parameters, environment.common, "/param/th/EnKey.prm");
}

void enemystrategy::Key::initializeItem() {
    load(physics.parameters, environment.common, "/param/th/EnThrowupKey.prm");
    flags |= 2;
    if (!valid(owner->source)) flags |= 0x14;
    std::string_view name;
    if (valid(owner->source) && owner->source.table->getValue(owner->source.row, "create_name", name) && name == "3_1") {
        if (!environment.globalSound) throw std::runtime_error("Original key creation sound is not bound.");
        environment.globalSound(0x381c);
    }
}

void enemystrategy::Key::beginActive() {
    beginFinite();
}

void enemystrategy::Key::updateActive() {
    auto& object = actor();
    if (!(object.fieldE0 & 255)) return;
    physics.update();
    if (physics.state == 2) {
        if (std::fabs(object.velocity[1]) > 3) environment.sounds.play(0x1829, object.position, 0);
        ++groundedFrames;
    }
    const auto bits = environment.keyParameters.entries()[0].bits;
    float ignore;
    std::memcpy(&ignore, &bits, sizeof(ignore));
    const char* animation = nullptr;
    if (static_cast<float>(frames) <= ignore) {
        if (!valid(owner->source)) animation = "NOMAPCOL";
    } else animation = "NORMAL";
    if (animation && object.selectAnimation(animation)) {
        auto& values = dynamic_cast<characterowner::Owner&>(*owner).state();
        values.field8cc = 0;
        values.field8d0 = 0;
    }
    updateFinite();
}

void enemystrategy::Key::onCollected() {
    awarded = true;
}

std::uint32_t enemystrategy::Key::captureEnd(characterevent::Event&) {
    if (flags & 1) environment.suction.effects.create(0x46, actor().position, {});
    if (!awarded) return 0;
    std::uint32_t door;
    const auto& source = owner->source;
    if ((valid(source) && source.table->getValue(source.row, "OpenDoorNo", door)) ||
        (valid(itemSource) && itemSource.table->getValue(itemSource.row, "OpenDoorNo", door))) {
        auto* recipient = environment.collection.actors.actor(environment.collection.enemies.appearPoint(0).actor);
        if (recipient) playerevents::collectKey(dynamic_cast<playeractor::Player&>(*recipient),
            static_cast<std::uint8_t>(door), environment.suction.flags);
    }
    return 1;
}
