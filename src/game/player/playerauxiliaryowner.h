#pragma once

#include "game/player/playerweaponowner.h"
#include "game/player/playervacuumeffects.h"

namespace playerresources {
    class AuxiliaryOwner {
    public:
        AuxiliaryOwner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
            playermodels::VacuumEffectGlobals& globals, playermodels::PipeParameters& pipeParameters);
        ~AuxiliaryOwner();
        std::array<std::uint32_t, 3> fields{};
        std::unique_ptr<playermodels::VacuumEffectOwner> vacuum;
        std::unique_ptr<playerweapons::Owner> weapon;
        float field14 = 0;
        std::array<float, 3> field18{}, field24{}, field30{0, 0, 1};
        std::uint8_t field3C = 0, field3D = 0;
    };

    struct AuxiliaryState {
        AuxiliaryState() {}
        void* field0 = nullptr;
        void* field4 = nullptr;
        std::uint8_t field8 = 0;
        std::array<std::uint8_t, 0x23> fields9;
        float field2C = 0;
        std::uint8_t field30 = 0, field31 = 0;
    };
}
