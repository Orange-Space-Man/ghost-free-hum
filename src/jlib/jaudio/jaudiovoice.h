#pragma once

#include "jlib/jaudio/jaudioresources.h"
#include <memory>

namespace jaudio {
    struct EnvelopePoint {
        unsigned curve = 0;
        unsigned duration = 0;
        std::int16_t target = 0;
    };

    struct Oscillator {
        unsigned destination = 0;
        float rate = 1;
        float scale = 1;
        float offset = 0;
        std::vector<EnvelopePoint> attack;
        std::vector<EnvelopePoint> release;
    };

    struct Envelope {
        float value = 0;
        float target = 0;
        float remaining = 0;
        float step = 0;
        unsigned position = 0;
        unsigned curve = 0;
        bool holding = false;
        bool released = false;
        bool ended = false;
        bool fallbackRelease = false;
        void release(const Oscillator& oscillator, unsigned duration);
        float advance(const Oscillator& oscillator);
    };

    struct Wave {
        unsigned rootKey = 60;
        float sampleRate = 32028.5f;
        std::vector<std::int16_t> samples;
        std::vector<std::int16_t> loop;
    };

    struct InstrumentVoice {
        std::shared_ptr<const Wave> wave;
        float volume = 1;
        float pitch = 1;
        float basePitch = 1;
        bool percussion = false;
        float pan = 0.5f;
        unsigned release = 1000;
        Oscillator envelope;
    };

    std::int16_t resampleWave(const Wave& wave, std::uint64_t position, unsigned ratio,
        const std::array<std::int16_t, 256>& coefficients);

    bool decodeAfc(const std::vector<std::uint8_t>& data, unsigned count,
        const std::array<std::int16_t, 32>& coefficients, std::int16_t history1, std::int16_t history2,
        std::vector<std::int16_t>& samples, std::string& error);

    class VoiceBank {
    public:
        bool load(gameassets::Package& package, const Resources& resources, std::string& error, unsigned instrumentBank = 4,
            unsigned waveSystem = 1, const std::vector<unsigned>& additionalWaveSystems = {});
        InstrumentVoice voice(unsigned bank, unsigned program, unsigned key, unsigned velocity);
        float keyPitch(const InstrumentVoice& voice, unsigned key) const;
        float pitch(float bend, unsigned range) const;
        float pan(float value) const;
        std::int16_t resample(const Wave& wave, std::uint64_t position, unsigned ratio) const;
        unsigned channelLevel = 0;
        unsigned outputLevel = 0;
        Oscillator vibrato;

    private:
        struct WaveLocation {
            std::string archive;
            std::size_t header = 0;
        };
        std::shared_ptr<const Wave> wave(unsigned id);
        void loadWaveSystem(gameassets::Package& package, const Resources& resources, unsigned id);
        std::vector<std::uint8_t> bankData;
        unsigned bankId = 4;
        unsigned waveId = 1;
        std::map<unsigned, std::vector<std::uint8_t>> waveSystems;
        std::map<unsigned, WaveLocation> locations;
        std::map<unsigned, std::shared_ptr<const Wave>> waves;
        std::map<std::string, std::vector<std::uint8_t>> archives;
        std::array<float, 128> pitches{};
        std::array<float, 64> finePitches{};
        std::array<std::int16_t, 32> coefficients{};
        std::array<std::int16_t, 256> resampling{};
        std::array<float, 257> sine{};
    };
}
