#pragma once

#include "jlib/jaudio/jaudioeffectconfig.h"

namespace jaudio {
    using DspBlock = std::array<std::int16_t, 80>;
    using DspBuses = std::array<DspBlock, 12>;

    void mixVoice(DspBlock& destination, const DspBlock& source, std::int16_t current, std::int16_t target);
    std::int16_t scaleOutput(std::int16_t sample, std::int16_t gain);

    class EffectProcessor {
    public:
        bool configure(const EffectConfiguration& configuration, unsigned preset, std::string& error);
        void reset();
        void begin(DspBuses& buses);
        void end(const DspBuses& buses);
        bool hasTail() const;

    private:
        struct Line {
            EffectLine configuration;
            std::vector<DspBlock> delay;
            std::array<std::int16_t, 8> history{};
            std::size_t position = 0;
        };
        std::array<Line, 4> lines;
    };

    class DspMixer {
    public:
        bool configure(const EffectConfiguration& configuration, std::string& error);
        void reset();
        void begin();
        void mix(unsigned destination, const DspBlock& source, std::int16_t current, std::int16_t target);
        void end(std::array<std::int16_t, 160>& samples, std::int16_t outputLevel);
        bool hasTail() const;

    private:
        EffectProcessor effects;
        DspBuses buses{};
    };
}
