#pragma once

#include "game/player/playerauxiliaryowner.h"
#include "game/player/playercontrollerstate.h"
#include "game/player/playerresourcepools.h"
#include "game/player/playervacuumcomponent.h"
#include "game/player/playerrecordwriteback.h"

namespace enemies { class Manager; }
namespace actors { class Registry; }

namespace players {
    struct Globals {
        playermodels::VacuumEffectGlobals effects;
        playermodels::PipeParameters pipe;
        playercomponents::VacuumGlobals vacuum;
        float field8BF8 = 0;
        std::uint32_t field8C28 = 0;
    };

    class OwnedObject {
    public:
        virtual ~OwnedObject() = default;
    };

    class Manager {
    public:
        Manager(const gamearchive::Archive& common, const j3dmodel::MaterialDefaults& defaults, unsigned mapMode,
            const gamearchive::Archive* mapArchive, Globals& globals, playerinput::MotorState& motors);
        ~Manager();
        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        void initializeEntries();
        void initializeAuxiliary(const std::vector<std::uint8_t>& executable);
        void bindActors(enemies::Manager& enemies, actors::Registry& actors,
            const std::array<playerrecord::Record*, 2>& playerRecords, playerinput::Pad* pad,
            void* field798, std::int32_t& audioLife);
        std::array<std::unique_ptr<playerinput::ControllerState>, 4> controllers;
        std::unique_ptr<playerresources::EffectState1C> state;
        std::unique_ptr<playerresources::PointPool> points;
        std::unique_ptr<playerresources::EffectPool80> effects80;
        std::unique_ptr<playerresources::EffectPool20> effects20;
        std::unique_ptr<playermodels::VacuumEntries> entries;
        std::unique_ptr<playerresources::CurveOwner> curves;
        std::unique_ptr<playerresources::RecordPool44> records;
        std::unique_ptr<playermodels::SharedModels> models;
        std::unique_ptr<playerresources::AuxiliaryOwner> auxiliary;
        std::unique_ptr<playerresources::AuxiliaryState> auxiliaryState;
        std::unique_ptr<OwnedObject> field3C;
        std::uint16_t field40 = 0;

    private:
        Globals& globals;
    };
}
