#pragma once

#include "game/audio/gameplayaudiocontext.h"
#include "game/audio/gameplaymusicrouting.h"
#include "game/audio/gameplayroomaudio.h"
#include "jlib/jaudio/jaudioplayback.h"
#include <mutex>

namespace gameplaymusic {
    class Owner final : private Services {
    public:
        Owner(gameassets::Package& package, const jaudio::Resources& resources,
            const gameplayaudio::Context& context, const std::int32_t& playerHealth);
        void setRoom(unsigned room, std::int16_t soundPolygon);
        void notifyRoom(unsigned packedRoom, unsigned mapMode);
        void start();
        bool requestLab(std::uint32_t audioFrame);
        bool requestTraining(std::uint32_t audioFrame);
        bool requestDeath(std::uint32_t audioFrame);
        bool requestEntrance(std::uint32_t audioFrame);
        bool requestFirstKey(std::uint32_t audioFrame);
        bool requestParlor(std::uint32_t audioFrame);
        void updateRequests(std::uint32_t audioFrame, bool enabled = true, bool paused = false);
        bool requestItem(std::uint32_t audioFrame);
        void stop();
        void setCutsceneActive(bool active);
        void updateSequenceControls();
        void retainSection();
        bool mix(jaudio::DspMixer& destination, std::string& error);
        const State& routing() const;
        const jaudio::Playback& playback() const;
        unsigned currentSound() const;

    private:
        mutable std::recursive_mutex mutex;
        bool hasMansionSequence() const override;
        unsigned sequenceState() const override;
        short roomSize() override;
        short roomMusicCode() override;
        void setTrackMuted(unsigned track, bool muted) override;
        void setTrackVolume(unsigned track, float volume, unsigned frames) override;
        void refreshContext();
        bool deferRequest(unsigned id);
        gameassets::Package& package;
        const jaudio::Resources& resources;
        const gameplayaudio::Context& context;
        const std::int32_t& health;
        jaudio::Playback sequence;
        jaudio::Playback fanfare;
        jaudio::SequenceVolume sequenceVolume;
        bool fanfareActive = false, resumeMansion = false;
        State state;
        RoomAudio roomAudio;
        gameplayaudio::RoomParameters roomParameters;
        unsigned room = 0xffffffff, soundState = 0;
        std::int16_t soundPolygon = -1;
        unsigned lastSection = 0;
        unsigned preparedSound = 0, activeSound = 0;
        unsigned pendingSound = 0;
        std::uint32_t lastRequestFrame = 0;
    };
}
