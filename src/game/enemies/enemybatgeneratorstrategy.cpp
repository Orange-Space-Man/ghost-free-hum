#include "game/enemies/enemybatgeneratorstrategy.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

enemystrategy::BatGenerator::BatGenerator(enemies::Runtime& runtime, const roomlocation::Rooms& rooms, bool second) :
    runtime(runtime), rooms(rooms), second(second) {}

charactergenerator::Generator& enemystrategy::BatGenerator::generator() const {
    if (!context) throw std::runtime_error("Original bat strategy has no generator owner.");
    return *static_cast<charactergenerator::Generator*>(context);
}

void enemystrategy::BatGenerator::initializeBehavior() {
    auto& parent = generator();
    if (state == 0) {
        const auto& source = parent.source;
        const char* names[]{"pos_x", "pos_y", "pos_z"};
        for (unsigned i = 0; i < 3; ++i) {
            if (!source.table || source.row < 0 || !source.table->getValue(source.row, names[i], position[i])) {
                throw std::runtime_error("Original bat generator requires its placement position.");
            }
        }
        radius = static_cast<float>(static_cast<std::uint32_t>(parent.parameter("arg0")));
        if (second) parameter = static_cast<std::uint32_t>(parent.parameter("arg1"));
        const auto& bounds = rooms.entries().at(runtime.resolveRoom(position) >> 24).bounds;
        for (unsigned i = 0; i < 3; ++i) {
            minimum[i] = static_cast<float>(bounds[i]);
            maximum[i] = static_cast<float>(bounds[i + 3]);
        }
        if (second) parent.parameter("max_enemy");
    } else if (state == 2) {
        const float random = static_cast<float>(runtime.random()) / 4294967296.0f;
        const auto count = static_cast<std::int32_t>(static_cast<std::uint32_t>(parent.parameter("appear_time")) + 1u);
        initialDelay = static_cast<std::int32_t>(static_cast<double>(static_cast<float>(count)) * random);
    }
}

void enemystrategy::BatGenerator::behavior() {
    if (state == 0) {
        setNextState(2);
    } else if (state == 1) {
        if (frames > 10) setNextState(2);
    } else if (state == 2) {
        if (!runtime.roomVisible(runtime.resolveRoom(position), 0)) return;
        auto& parent = generator();
        if (static_cast<std::uint32_t>(parent.parameter("appear_time")) < frames) setNextState(1);
        auto* child = parent.create(parent.type, -1);
        if (!child) return;
        auto* spawn = dynamic_cast<BatSpawn*>(child->ownership().strategy.get());
        if (!spawn || !spawn->initializeBat(second, radius, position, parameter)) return;
        auto& actor = dynamic_cast<actors::MoveActor&>(child->actor());
        const float random = static_cast<float>(runtime.random()) / 4294967296.0f;
        const auto yaw = static_cast<std::int16_t>(static_cast<std::int32_t>(1.0f + 65535.0f * random));
        actor.rotation = {0, yaw, 0};
        actor.previousRotation = actor.rotation;
        actor.setVelocity({});
        actor.fieldE0 &= 0xffffff00;
        actor.fieldE4 &= 0xffffff00;
        actor.flags |= 0xc000;
        actor.blendFramesRemaining = 0;
        actor.blendFramesTotal = 1;
    }
}
