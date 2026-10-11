#include "game/enemies/enemygoldratgeneratorstrategy.h"

#include <stdexcept>

enemystrategy::GoldRatGenerator::GoldRatGenerator(enemies::Runtime& runtime) : runtime(runtime) {}

void enemystrategy::GoldRatGenerator::behavior() {
    if (state == 0) {
        setNextState(0x103);
        return;
    }
    if (state != 0x104) return;
    if (!context) throw std::runtime_error("Original gold-rat strategy has no generator owner.");
    auto& parent = *static_cast<charactergenerator::Generator*>(context);
    auto* child = parent.create(parent.type, -1);
    if (!child) {
        setNextState(0x103);
        return;
    }
    const float random = static_cast<float>(runtime.random()) / 4294967296.0f;
    auto& path = parent.path();
    if (!path.attached() || !path.count()) throw std::runtime_error("Original gold-rat generator requires a nonempty path.");
    int point = static_cast<int>(static_cast<double>(static_cast<float>(static_cast<std::int32_t>(path.count()))) * random);
    if (point == previousPoint) point = (point + 1) % static_cast<int>(path.count());
    previousPoint = point;
    std::array<float, 3> position;
    const char* names[]{"pnt0_x", "pnt0_y", "pnt0_z"};
    for (unsigned i = 0; i < 3; ++i) {
        if (!path.getValue(point, names[i], position[i])) throw std::runtime_error("Original gold-rat path coordinate is missing.");
    }
    child->actor().setPosition(position);
}

void enemystrategy::GoldRatGenerator::receive(charactergenerator::Event&) {}
