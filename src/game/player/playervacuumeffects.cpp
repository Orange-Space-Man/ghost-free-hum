#include "game/player/playervacuumeffects.h"
#include <cmath>
#include <stdexcept>

void playermodels::VacuumEffectGlobals::initializeOwner() {
    auto& first = parameters[0];
    first.field48 = 15;
    first.field5C = 1;
    first.field1D = 1;
    first.field24 = {0.36f, 0.5f, 0.45f};
    first.field30 = first.field34 = 1;
    first.field64 = 255;
    first.field1C = 1;
    first.field60 = 0.05f;
    first.field54 = 20;
    first.field58 = 5;
    first.field4C = 0;
    auto& second = parameters[1];
    second.field14 = 4;
    second.field48 = 15;
    second.field5C = 1;
    second.field1D = 1;
    second.field24 = {0.93f, 0.25f, 0.1f};
    second.field30 = second.field34 = 1;
    second.field64 = 255;
    second.field1C = 1;
    second.field60 = 0.05f;
    second.field54 = 30;
    second.field58 = 5;
    second.field4C = 0;
}

playermodels::VacuumEffectOwner::VacuumEffectOwner(const SharedModels& shared, VacuumEffectGlobals& globals) {
    model = std::make_unique<VacuumModel>(shared, 9, 0);
    globals.initializeOwner();
}

void playermodels::VacuumEffectOwner::start(const Input& value,
    const std::function<void(unsigned, const std::array<float, 3>&)>& effect) {
    if (!state && !effect) throw std::logic_error("Vacuum activation requires its particle service.");
    input = value;
    if (state) return;
    PathParameters initial;
    initial.fields[0] = &input.origin;
    initial.fields[1] = &input.target;
    model->initialize(0, initial, nullptr);
    effect(3, input.target);
    state = 1;
}

void playermodels::VacuumEffectOwner::stop(const std::function<void(const VacuumPoint&, unsigned, float)>& particles) {
    if (!particles) throw std::logic_error("Vacuum release requires its point-particle service.");
    for (unsigned i = 0; i < model->count; ++i) particles(model->points[i], 24, 40);
    state = 0;
}

playermodels::VacuumParameters playermodels::VacuumEffectOwner::parameters(const VacuumEffectGlobals& globals) const {
    auto result = globals.parameters[input.alternate ? 1 : 0];
    result.fields[0] = &input.origin;
    result.fields[1] = &input.target;
    result.fields[2] = &input.direction;
    result.fields[4] = input.alternate ? &input.targetDirection : nullptr;
    if (input.alternate) result.field20 = input.distance;
    else {
        float squared = 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            const float difference = input.origin[axis] - input.target[axis];
            squared += difference * difference;
        }
        result.field20 = std::sqrt(squared);
    }
    result.field40 = input.angle;
    result.field44 = input.room;
    return result;
}

playermodels::VacuumTracking::VacuumTracking() {
    reset();
}

void playermodels::VacuumTracking::reset() {
    field0 = {0, 0, 0};
    fieldC = {0, 0, 1};
    field18 = {0, 0, 0};
    field24 = field28 = 0;
    field30 = field31 = 0;
    field2C = 40;
    field34 = 0;
    field38 = field39 = 0;
}

void playermodels::VacuumTrackingBlend::reset() {
    field0 = 0;
    field4 = 0;
    field8 = 0;
}

playermodels::VacuumEntry::VacuumEntry(const SharedModels& shared, VacuumEffectGlobals& globals) {
    effect = std::make_unique<VacuumEffectOwner>(shared, globals);
}

playermodels::VacuumEntries::VacuumEntries(const SharedModels& shared, VacuumEffectGlobals& globals) {
    for (auto& entry : entries) entry = std::make_unique<VacuumEntry>(shared, globals);
    reset();
}

playermodels::VacuumEntries::~VacuumEntries() {
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) it->reset();
}

void playermodels::VacuumEntries::reset() {
    fieldC90 = fieldC94 = 0;
    fieldA38.fill(0);
    fieldA50.fill(0);
    fieldBB8.fill(0);
    fieldBD0.fill(0);
    fieldBE8.fill(0);
}

void playermodels::VacuumEffectOwner::updateMotion(const VacuumEffectGlobals& globals, const actormodel::InitialPose& pose,
    const PathCollision& collision, const VacuumFeedback& feedback) {
    if (state) model->updateMotion(parameters(globals), pose, collision, feedback);
}
