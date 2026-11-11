#pragma once

#include "game/actors/actorregistry.h"
#include "game/actors/charactercreationruntime.h"
#include "game/scene/gamearchiveresources.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/audio/gameplayaudiocontext.h"
#include "game/audio/gameplaymusicowner.h"
#include "game/effects/modeleffectmanager.h"
#include "game/effects/particlescene.h"
#include "game/player/playereventdispatch.h"
#include "game/events/missioneventactivation.h"
#include "game/events/jmpentryactivation.h"
#include "game/world/doorstate.h"
#include "game/enemies/enemygeneratormanager.h"
#include "game/effects/particleglowpool.h"
#include "game/player/playerpadsampler.h"
#include "game/player/playerdoorinteraction.h"
#include "game/player/playerladderinteraction.h"
#include "game/player/playerfinishframe.h"
#include "game/player/playerliferecovery.h"
#include "game/player/playerlateframe.h"
#include "game/player/playervelocityframe.h"
#include "game/player/playerenvironmentframe.h"
#include "game/player/playermainframe.h"

namespace playeractions { struct Input; }
namespace roomfurniture { class Owner; }

namespace gameplay {
    struct PlayerMainServices {
        playerpresentation::Services* presentation;
        playerscanner::Services* scanner;
        playeractor::TimerServices timers;
        const std::array<playermotion::PlatformMotion, 2>& platforms;
        bool inputAvailable = true;
        bool keyPresentationActive = false;
        std::function<void(bool, bool, std::uint8_t)> presentKey;
        std::function<void(playeractor::Player&, bool)> scannerTransition;
    };

    struct PlayerMotionServices {
        playerevents::LifeEnvironment& life;
        bool doubledDamage;
        unsigned frame;
        const std::array<playermotion::PlatformMotion, 2>& platforms;
        std::function<void(std::uint32_t)> globalSound;
    };

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
        unsigned playerCount = 1;
        unsigned variant = 0;
        std::function<void(const enemies::Manager&)> prepareRooms;
        std::function<void(const std::array<std::uint8_t, 262>&)> updateActorResources;
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
        playerevents::LifeEnvironment& playerLife();
        particles::Scene& particles();
        modeleffects::Manager& modelEffects();
        mapcollision::State& collision();
        roomlighting::Pool& lights();
        std::shared_ptr<jaudio::PositionalPlayback> playback();
        const gameplayaudio::Context& audioContext() const;
        gameplaymusic::Owner& music();
        gameflags::State& flags();
        const playerinput::Pad& controller() const;
        bool roomCleared(unsigned room) const;
        void updateOwner(unsigned slot);
        void updatePlacement(std::int32_t elapsedSeconds, bool force = true);
        void updateGenerators();
        void updateContacts();
        void updatePoses();
        void sampleInput(const playerinput::PadStatus& pad, std::int16_t port);
        void updatePlayerInput(unsigned index, bool inputAvailable,
            const std::function<void(playeractor::Player&, bool)>& scannerTransition);
        void updatePlayerOrientation(unsigned index, const playercomponents::AttachedOutput& attachment, unsigned frame);
        void updatePlayerMotion(unsigned index, const PlayerMotionServices& services);
        void updatePlayerMain(unsigned index, const PlayerMainServices& services);
        void bindPlayerFurniture(roomfurniture::Owner& furniture);
        void updatePlayerFurniture(unsigned index);
        void updateFurniture(unsigned frame);
        void updateDoors(unsigned frame);
        void updatePlayerLight(unsigned index);
        void updatePlayerAudio(unsigned index);
        void updatePlayerLate(unsigned index, const playeractor::LateFrameServices& services);
        void completePlayerFrame(const playeractor::LateFrameServices& services, bool resetLights = true);
        void setAudioInterfaceFrame(std::uint32_t frame);
        const std::array<std::uint32_t, 2>& playerFootstepFrames() const;
        bool beginPlayerCapture(std::int32_t index, const std::array<float, 3>& position, std::int32_t recipient);
        void updatePlayerCapture(std::int32_t index);
        bool releasePlayerCapture(std::int32_t index, const std::array<float, 3>* position,
            const std::array<float, 3>* direction, bool launched);
        doors::State& doors();
        charactergenerator::Manager& generators();
        particles::GlowPool& glows();
        unsigned candleCount() const;
        bool eventEffect(std::string_view placement, std::string_view command);
        const jmp::ActiveEntries& placements() const;
        const gameevent::MissionActivation& missionEvents() const;
        gameevent::MissionActivation& missionEvents();
        void updateCaptures(const playermodels::VacuumPath* path, const actorcapture::Vector* suctionDirection,
            actormodel::Matrix& retainedTransform, const actorcapture::Vector* originalScratch = nullptr,
            const float* originalRegister = nullptr, const bool* emptyCompletion = nullptr);
        void updateAudio(const std::array<float, 12>& listener);
        playeractions::Input preparePlayerAction(unsigned index);
        playerenvironment::Services preparePlayerEnvironment(unsigned index);
        playerinteraction::DoorServices preparePlayerDoor(unsigned index);
        playerinteraction::LadderServices preparePlayerLadder(unsigned index);
        playeractor::FinishServices preparePlayerFinish(unsigned index, playerevents::LifeEnvironment& life,
            bool doubledDamage);

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
