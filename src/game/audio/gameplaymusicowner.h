#pragma once

#include "game/audio/gameplayaudiocontext.h"
#include "game/audio/gameplaymusicrouting.h"
#include "jlib/jaudio/jaudioplayback.h"

namespace gameplaymusic {
    class Owner final : private Services {
    public:
        Owner(gameassets::Package& package, const jaudio::Resources& resources,
            const gameplayaudio::Context& context, const std::int32_t& playerHealth);
        void setRoom(unsigned room, std::int16_t soundPolygon);
        void start();
        void stop();
        void setCutsceneActive(bool active);
        void updateSequenceControls();
        void retainSection();
        bool mix(jaudio::DspMixer& destination, std::string& error);
        const State& routing() const;
        const jaudio::Playback& playback() const;

    private:
        bool hasMansionSequence() const override;
        unsigned sequenceState() const override;
        short roomSize() override;
        short roomMusicCode() override;
        void setTrackMuted(unsigned track, bool muted) override;
        void setTrackVolume(unsigned track, float volume, unsigned frames) override;
        void refreshContext();
        gameassets::Package& package;
        const jaudio::Resources& resources;
        const gameplayaudio::Context& context;
        const std::int32_t& health;
        jaudio::Playback sequence;
        State state;
        gameplayaudio::RoomParameters roomParameters;
        unsigned room = 0xffffffff, soundState = 0;
        std::int16_t soundPolygon = -1;
        unsigned lastSection = 0;
        bool prepared = false;
    };
}
