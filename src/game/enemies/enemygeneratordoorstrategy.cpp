#include "game/enemies/enemygeneratordoorstrategy.h"
#include "game/actors/charactergenerator.h"

#include <stdexcept>

namespace {
    charactergenerator::Generator& generator(void* context) {
        if (!context) throw std::runtime_error("Original door generator has no owning generator.");
        return *static_cast<charactergenerator::Generator*>(context);
    }
}

void enemystrategy::GeneratorDoor::initializeBehavior() {
    if (state != 0) return;
    const auto& source = generator(context).source;
    if (source.table && source.row >= 0) {
        source.table->getValue(source.row, "pos_x", position[0]);
        source.table->getValue(source.row, "pos_y", position[1]);
        source.table->getValue(source.row, "pos_z", position[2]);
    }
    setNextState(1);
}

void enemystrategy::GeneratorDoor::behavior() {
    if (state != 1) return;
    auto& parent = generator(context);
    if (auto* child = parent.create(parent.type, -1)) child->actor().setPosition(position);
}
