#include "game/player/playerauxiliaryowner.h"

playerresources::AuxiliaryOwner::AuxiliaryOwner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
    playermodels::VacuumEffectGlobals& globals, playermodels::PipeParameters& pipeParameters) {
    vacuum = std::make_unique<playermodels::VacuumEffectOwner>(shared, globals);
    weapon = std::make_unique<playerweapons::Owner>(executable, shared, pipeParameters);
}

playerresources::AuxiliaryOwner::~AuxiliaryOwner() {
    weapon.reset();
    vacuum.reset();
}
