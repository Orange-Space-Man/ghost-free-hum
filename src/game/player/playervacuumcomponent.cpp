#include "game/player/playervacuumcomponent.h"

void playercomponents::VacuumGlobals::initialize() {
    field8C18 = 13;
    auto& first = parameters[0];
    first.field20 = 200;
    first.field24 = {0.77f, 0.2f, 0.3f};
    first.field30 = first.field34 = 1;
    first.field5C = 0.2f;
    first.field54 = 74;
    first.field58 = 18.5f;
    first.field64 = 0;
    first.field1C = 1;
    first.field4C = 0;
    first.field60 = 0;
    first.field48 = 15;
    auto& second = parameters[1];
    second.field20 = 800;
    second.field24 = {0.36f, 0.5f, 0.45f};
    second.field30 = second.field34 = 1;
    second.field5C = 1;
    second.field54 = 40;
    second.field58 = 5;
    second.field64 = 70;
    second.field1C = 1;
    second.field60 = 0.5f;
}

playercomponents::Vacuum::Vacuum(const playermodels::SharedModels& shared, VacuumGlobals& globals) {
    model = std::make_unique<playermodels::VacuumModel>(shared, 10, 0);
    globals.initialize();
}
