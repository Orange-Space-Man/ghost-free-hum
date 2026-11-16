#include "game/enemies/enemytrainingservices.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

void enemystrategy::TrainingEnvironment::openGate(unsigned index) {
    const auto name = "gate" + std::to_string(index + 1);
    for (const auto& slot : enemies.slots()) {
        if (slot.state != 2 || !slot.owner) continue;
        const auto& source = slot.owner->source;
        std::string_view codeName;
        if (!source.table || source.row < 0 || !source.table->getValue(source.row, "CodeName", codeName))
            codeName = types.name(slot.owner->actor().type());
        if (codeName != name) continue;
        auto& actor = dynamic_cast<actors::MoveActor&>(slot.owner->actor());
        actor.selectAnimation("OPEN");
        auto matrix = pose.rotation(actor.rotation);
        for (unsigned axis = 0; axis < 3; ++axis) matrix[axis * 4 + 3] = actor.position[axis];
        models.create(1, matrix);
        particles.create(4, actor.position, {0, 0, 0});
        return;
    }
}

void enemystrategy::TrainingEnvironment::createChild(charactergenerator::Generator& parent, unsigned index) {
    auto& path = parent.path();
    if (!path.attached()) return;
    std::array<float, 3> position{};
    const char* names[]{"pnt0_x", "pnt0_y", "pnt0_z"};
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!path.getValue(index, names[axis], position[axis]))
            throw std::runtime_error("Training release path coordinate is missing.");
    if (!runtime.roomVisible(runtime.resolveRoom(position), 0)) return;
    if (auto* child = parent.create(parent.type, -1)) {
        auto& actor = dynamic_cast<actors::MoveActor&>(child->actor());
        actor.setPosition(position);
        actor.fieldE0 &= 0xffffff00;
        actor.fieldE4 &= 0xffffff00;
    }
}

enemystrategy::TrainingServices enemystrategy::TrainingEnvironment::bind(TrainingGenerator& strategy) {
    return {[this] { return runtime.random(); }, [this] { return allGates; },
        [this](unsigned index) { openGate(index); },
        [this, &strategy](unsigned index) {
            if (!strategy.context) throw std::runtime_error("Training release requires its generator owner.");
            createChild(*static_cast<charactergenerator::Generator*>(strategy.context), index);
        }};
}
