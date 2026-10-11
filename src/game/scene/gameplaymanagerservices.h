#pragma once

#include "game/actors/actorregistry.h"
#include "game/actors/charactercreationruntime.h"
#include "game/scene/gamearchiveresources.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/effects/modeleffectmanager.h"
#include "game/effects/particlescene.h"
#include "game/player/playereventdispatch.h"
#include "game/events/missioneventactivation.h"
#include "game/events/jmpentryactivation.h"
#include "game/world/doorstate.h"
#include "game/enemies/enemygeneratormanager.h"
#include "game/effects/particleglowpool.h"
#include "game/player/playerpadsampler.h"

namespace playeractions { struct Input; }

namespace gameplay {
    struct ManagerDependencies {
        const std::vector<std::uint8_t>& executable;
        gameassets::Package& package;
        const jaudio::Resources& audio;
        const gamearchive::Archive& common;
        const gamearchive::Archive& map;
        const mission::Resources& mission;
        const j3dmodel::MaterialDefaults& materials;
        const actors::Construction& actors;
        const actorcollision::Catalog& collisions;
        const enemytypes::Catalog& types;
        const enemycreation::Catalog& creation;
        const enemystrategy::Catalog& strategies;
        gameplaycamera::CameraOwner& camera;
        const std::vector<std::uint32_t>& roomFlags;
        const roomtraversal::State& visibility;
        gameentry::State& entry;
        gameflags::State& flags;
        playerinput::MotorState& motors;
        playerinput::Pad& pad;
        std::array<playerrecord::Record*, 2> records;
        std::uint32_t& randomState;
        std::int32_t& audioLife;
        unsigned appearPoint = 0;
        unsigned playerCount = 2;
        unsigned variant = 0;
        void* playerContext = nullptr;
        std::function<void(const enemies::Manager&)> prepareRooms;
    };

    class ManagerServices {
    public:
        explicit ManagerServices(const ManagerDependencies& dependencies);
        ~ManagerServices();
        ManagerServices(const ManagerServices&) = delete;
        ManagerServices& operator=(const ManagerServices&) = delete;
        enemies::Manager& enemies();
        actors::Registry& actors();
        players::Manager& players();
        playerevents::Dispatcher& playerEvents();
        particles::Scene& particles();
        modeleffects::Manager& modelEffects();
        mapcollision::State& collision();
        roomlighting::Pool& lights();
        std::shared_ptr<jaudio::PositionalPlayback> playback();
        void updateOwner(unsigned slot);
        void updatePlacement(std::int32_t elapsedSeconds);
        void updateGenerators();
        void updateContacts();
        void sampleInput(const playerinput::PadStatus& pad, std::int16_t port);
        doors::State& doors();
        charactergenerator::Manager& generators();
        particles::GlowPool& glows();
        const jmp::ActiveEntries& placements() const;
        const gameevent::MissionActivation& missionEvents() const;
        void updateCaptures(const playermodels::VacuumPath* path, const actorcapture::Vector* suctionDirection,
            actormodel::Matrix& retainedTransform, const actorcapture::Vector* originalScratch = nullptr,
            const float* originalRegister = nullptr, const bool* emptyCompletion = nullptr);
        void updateAudio(const std::array<float, 12>& listener);
        playeractions::Input preparePlayerAction(unsigned index);

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
