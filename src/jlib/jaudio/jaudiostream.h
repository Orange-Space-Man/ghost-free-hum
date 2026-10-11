#pragma once

#include "resources/gameassetpackage.h"
#include <array>

namespace jaudio {
    struct StreamAudio {
        unsigned sampleRate = 0, frameRate = 0;
        double playbackRate = 0;
        double framesPerSample = 0;
        std::vector<std::int16_t> samples;
    };

    bool decodeStream(const std::vector<std::uint8_t>& data, const std::array<std::int16_t, 32>& coefficients,
        StreamAudio& audio, std::string& error);
    bool readStream(gameassets::Package& package, const std::string& path, StreamAudio& audio, std::string& error);
}
