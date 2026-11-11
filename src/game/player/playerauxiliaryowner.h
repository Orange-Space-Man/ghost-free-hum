#pragma once

#include "game/player/playerweaponowner.h"
#include "game/player/playervacuumeffects.h"
#include "game/actors/moveactor.h"
#include "game/enemies/enemystrategycatalog.h"

namespace playerresources {
    struct AuxiliaryFrameServices {
        const actors::Construction& actors;
        const enemystrategy::Catalog& strategies;
        const playermodels::PipeParameters& pipe;
        const playermodels::VacuumEffectGlobals& vacuum;
        playermodels::PathCollision collision;
        playermodels::VacuumFeedback feedback;
        std::function<std::uint32_t()> random;
        std::function<void(unsigned, const std::array<float, 3>&)> effect;
        std::function<void(unsigned, const std::array<float, 3>&, float)> sound;
    };

    class AuxiliaryOwner {
    public:
        AuxiliaryOwner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
            playermodels::VacuumEffectGlobals& globals, playermodels::PipeParameters& pipeParameters);
        ~AuxiliaryOwner();
        void attach(actors::MoveActor& owner, actors::MoveActor& head, actors::MoveActor& body);
        void startBeam(int mode, actors::MoveActor& target);
        void stopBeam(const std::function<void(const playermodels::VacuumPoint&, unsigned, float)>& particles);
        void update(const AuxiliaryFrameServices& services);
        actors::MoveActor* actor = nullptr;
        actors::MoveActor* head = nullptr;
        actors::MoveActor* body = nullptr;
        std::unique_ptr<playermodels::VacuumEffectOwner> vacuum;
        std::unique_ptr<playerweapons::Owner> weapon;
        float field14 = 0;
        std::array<float, 3> field18{}, field24{}, field30{0, 0, 1};
        std::uint8_t field3C = 0, field3D = 0;
        actors::MoveActor* target = nullptr;
        float beamStrength = 0;
        std::uint8_t beamMode = 0;

    private:
        const std::vector<std::uint8_t>& executable;
        std::array<float, 3> beamStrengths{};
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
