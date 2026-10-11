#include "game/player/playervacuumeffects.h"

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
