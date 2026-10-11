#include "game/player/playerweaponowner.h"

playerweapons::Owner::Owner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
    playermodels::PipeParameters& parameters) {
    first = std::make_unique<Part>(executable, actorType, firstType);
    second = std::make_unique<Part>(executable, actorType, secondType);
    pipe = std::make_unique<playermodels::PipeModel>(shared, 15);
    effect.flags = 0;
    parameters.field30 = 0;
    effect.field40 = 0;
    effect.field34 = 1;
    effect.field38 = effect.field3C = 0;
    parameters.field1D = 1;
    parameters.field18 = -1.6f;
    parameters.field24 = {0.95f, 0.8f, 0.8f};
    parameters.field34 = 0.5f;
    parameters.field38 = 0.88f;
    parameters.field1C = 1;
    parameters.field44 = 0.4f;
    parameters.field20 = 150;
    playermodels::PipeParameters initial;
    initial.fields[0] = &second->position;
    initial.fields[1] = &first->position;
    initial.field20 = 150;
    pipe->initialize(0, initial, nullptr);
    fieldC = 2;
}

playerweapons::Owner::~Owner() {
    pipe.reset();
    first.reset();
    second.reset();
}
