#include "game/enemies/enemytelesageneratorstrategy.h"
#include "game/actors/moveactor.h"

#include <cmath>
#include <stdexcept>

enemystrategy::TelesaGenerator::TelesaGenerator(enemies::Runtime& runtime) : runtime(runtime) {}

charactergenerator::Generator& enemystrategy::TelesaGenerator::generator() const {
    if (!context) throw std::runtime_error("Original Telesa strategy has no generator owner.");
    return *static_cast<charactergenerator::Generator*>(context);
}

void enemystrategy::TelesaGenerator::initializeBehavior() {
    auto& parent = generator();
    if (state == 0 && parent.source.table && parent.source.row >= 0) {
        const char* names[]{"pos_x", "pos_y", "pos_z"};
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (!parent.source.table->getValue(parent.source.row, names[axis], position[axis]))
                throw std::runtime_error("Original Telesa generator requires its placement position.");
        }
        runtime.resolveRoom(position);
    } else if (state == 2) {
        const float random = static_cast<float>(runtime.random()) / 4294967296.0f;
        const auto count = static_cast<std::int32_t>(static_cast<std::uint32_t>(parent.parameter("appear_time")) + 1u);
        appearanceFrame = static_cast<int>(static_cast<double>(static_cast<float>(count)) * random);
    }
}

void enemystrategy::TelesaGenerator::behavior() {
    auto& parent = generator();
    if (state == 0) {
        setNextState(1);
    } else if (state == 1) {
        if (frames > 10) setNextState(2);
    } else if (state == 2) {
        if (static_cast<std::uint32_t>(parent.parameter("appear_time")) < frames) {
            setNextState(1);
            return;
        }
        if (static_cast<std::uint32_t>(appearanceFrame) != frames || !parent.source.table || parent.source.row < 0) return;
        if (!runtime.roomVisible(runtime.resolveRoom(position), 0)) return;
        const auto player = runtime.playerPosition(0);
        const float x = player[0] - position[0], y = player[1] + 100.0f - position[1], z = player[2] - position[2];
        if (std::sqrt(x * x + y * y + z * z) < 200.0f) return;
        auto* child = parent.create(parent.type, -1);
        if (!child) return;
        auto& actor = dynamic_cast<actors::MoveActor&>(child->actor());
        actor.rotation = actor.previousRotation = {};
        actor.setVelocity({});
        actor.fieldE0 &= 0xffffff00;
        actor.fieldE4 &= 0xffffff00;
        actor.blendFramesRemaining = 0;
        actor.blendFramesTotal = 1;
        actor.flags |= 0xc000;
        actor.setPosition(position);
    }
}
