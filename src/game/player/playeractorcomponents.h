#pragma once

#include "game/player/playeractorstate.h"
#include "game/player/playermanager.h"
#include "game/player/playercomponentstate.h"
#include "game/player/playercontrolstate.h"
#include "game/player/playeractionstate.h"
#include "game/player/playerattachedstate.h"
#include "game/player/playerscannermodel.h"
#include "game/player/playerlightmodel.h"
#include "game/player/playergridmodels.h"

namespace playeractor {
    class Components {
    public:
        Components(const std::vector<std::uint8_t>& executable, actors::MoveActor& actor, const State& state,
            players::Manager& manager, players::Globals& globals, gameplaycamera::CameraOwner& camera);
        ~Components();
        Components(const Components&) = delete;
        Components& operator=(const Components&) = delete;
        std::unique_ptr<playercontrols::State> controls;
        std::unique_ptr<playercomponents::EmptyState> empty;
        std::unique_ptr<playercomponents::QuaternionState> quaternion;
        std::unique_ptr<playercomponents::DoorState> door;
        std::unique_ptr<playercomponents::LadderState> ladder;
        std::unique_ptr<playercomponents::State80> state80;
        std::unique_ptr<playermodels::ScannerModels> scanner;
        std::unique_ptr<playermodels::GridOwner> grids;
        std::unique_ptr<playerweapons::Owner> weapon;
        std::unique_ptr<playercomponents::Vacuum> vacuum;
        std::unique_ptr<playermodels::LightModel> light;
        std::unique_ptr<playeractions::Actions> actions;
        std::unique_ptr<playeractions::Reactions> reactions;
        std::unique_ptr<playercomponents::AttachedState> attached;
        std::unique_ptr<playercomponents::AttachedSingle> single;
        std::unique_ptr<playercomponents::FlagBuffer> flags;
        std::array<std::unique_ptr<playercomponents::BitBuffer>, 5> bits;
    };
}
