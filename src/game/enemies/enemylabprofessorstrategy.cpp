#include "game/enemies/enemylabprofessorstrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

void enemystrategy::LabProfessor::behavior() {
    if (!owner) throw std::runtime_error("Original lab professor strategy requires its character owner.");
    if (!materialHidden)
        materialHidden = dynamic_cast<actors::MoveActor&>(owner->actor()).setMaterialFlags(0, 2, true);
}
