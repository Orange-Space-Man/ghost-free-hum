#pragma once

#include "jlib/jaudio/jaudioresources.h"
#include "jlib/jaudio/jaudiostream.h"
#include <memory>

namespace jaudio { class PositionalPlayback; }

namespace startupaudio {
    void prepare(const std::filesystem::path& package, const jaudio::Resources& resources);
    void playLogo(const std::vector<std::int16_t>& samples);
    void playTitle();
    void playMenu();
    void playStream(const jaudio::StreamAudio& audio);
    void stop();
    unsigned streamFrame();
    double streamPosition();
    void playStartSound();
    void playMenuEffect(unsigned id);
    void bindPositionalPlayback(std::shared_ptr<jaudio::PositionalPlayback> playback);
    bool startSoundFinished();
    void update();
    bool titleFinished();
    bool finished();
    void close();
}
