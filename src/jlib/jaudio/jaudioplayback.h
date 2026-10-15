#pragma once

#include "jlib/jaudio/jaudiosequence.h"
#include "jlib/jaudio/jaudiosequencecontrols.h"
#include "jlib/jaudio/jaudiovoice.h"
#include "jlib/jaudio/jaudiodspmixer.h"

namespace jaudio {
    struct SpatialParameters {
        float volume = 1, pan = 0.5f, pitch = 1, fxmix = 0, dolby = 0;
    };

    class Playback {
    public:
        bool load(gameassets::Package& package, const Resources& resources, std::string& error);
        bool loadMenu(gameassets::Package& package, const Resources& resources, std::string& error);
        bool loadMansion(gameassets::Package& package, const Resources& resources, std::string& error);
        void setTrackMuted(unsigned track, bool muted);
        void setTrackVolume(unsigned track, float volume, unsigned frames);
        void setTrackPitch(unsigned track, float pitch, unsigned frames);
        void updateSequenceControls();
        const SequenceControls& sequenceControls() const;
        void setTempoScale(float value, unsigned frames);
        void setRootPort(unsigned port, std::uint16_t value);
        std::uint16_t rootPort(unsigned port) const;
        void setTrackPort(unsigned track, unsigned port, std::uint16_t value);
        bool loadStartSound(gameassets::Package& package, const Resources& resources, std::string& error);
        bool loadMenuEffect(gameassets::Package& package, const Resources& resources, unsigned id, std::string& error);
        bool loadPositionalEffect(gameassets::Package& package, const Resources& resources, unsigned id, std::string& error);
        void start();
        void stop();
        void release();
        bool render(std::array<std::int16_t, 160>& samples, std::string& error);
        bool mix(DspMixer& destination, std::string& error, const SpatialParameters& spatial = {});
        bool voicesFinished() const;
        std::int16_t outputLevel() const;
        bool finished() const;
        std::uint64_t notes() const;
        std::uint64_t ticks() const;
        unsigned peakVoices() const;

    private:
        struct Voice {
            unsigned track = 0;
            unsigned slot = 0;
            unsigned remaining = 0;
            InstrumentVoice instrument;
            Envelope envelope;
            Envelope vibrato;
            Oscillator vibratoDefinition;
            bool hasVibrato = false;
            bool ended = false;
            std::uint64_t position = 0;
            bool firstBlock = true;
            std::array<unsigned, 6> routes{};
            std::array<std::int16_t, 6> gains{};
        };
        void event(const SequenceEvent& event);
        float externalVolume(const Track& track) const;
        float externalPitch(const Track& track) const;
        bool loadMusic(gameassets::Package& package, const Resources& resources, unsigned id, std::string& error);
        SequenceControls controls;
        SequenceVolume tempoScale;
        bool tempoPending = false;
        std::array<std::array<std::uint16_t, 16>, 32> trackPorts{};
        std::array<std::uint16_t, 32> writtenPorts{};
        std::array<std::uint16_t, 32> pendingPorts{};
        VoiceBank bank;
        DspMixer mixer;
        Interpreter interpreter;
        std::vector<std::uint8_t> sequence;
        std::vector<Voice> voices;
        std::uint64_t noteCount = 0;
        unsigned voicePeak = 0;
        unsigned syncFlags = 0;
        std::size_t entryPoint = 0;
        unsigned tempo = 48;
        unsigned timebase = 120;
        bool effectDispatch = false;
    };
}
