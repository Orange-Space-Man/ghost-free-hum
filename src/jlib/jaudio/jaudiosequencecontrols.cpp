#include "jlib/jaudio/jaudiosequencecontrols.h"

#include <cmath>
#include <stdexcept>

float jaudio::blendTrackParameter(float local, float inherited, float weight, unsigned mode) {
    mode &= 255;
    if (mode == 0) return local;
    if (mode == 1) return inherited;
    if (mode != 2) throw std::runtime_error("Unsupported original track blend mode.");
    return std::fma(local, 1.0f - weight, inherited * weight);
}

void jaudio::SequenceVolume::set(float volume, std::uint32_t frames) {
    if ((!remaining && value == volume) || (remaining && target == volume)) return;
    target = volume;
    remaining = frames == 0 ? 1 : frames;
    decrement = remaining == 1 ? value - target : (value - target) / static_cast<float>(remaining);
}

void jaudio::SequenceVolume::update() {
    if (!remaining) return;
    if (--remaining) value -= decrement;
    else value = target;
}

void jaudio::SequenceControls::setVolume(unsigned track, float volume, std::uint32_t frames) {
    tracks.at(track & 0xff).volume.set(volume, frames);
}

void jaudio::SequenceControls::setMuted(unsigned track, bool muted) {
    tracks.at(track & 0xff).requestedMute = muted;
}

void jaudio::SequenceControls::setPitch(unsigned track, float pitch, std::uint32_t frames) {
    tracks.at(track & 0xff).pitch.set(pitch, frames);
}

void jaudio::SequenceControls::setPan(unsigned track, float pan, std::uint32_t frames) {
    tracks.at(track & 0xff).pan.set(pan, frames);
}

void jaudio::SequenceControls::setFxmix(unsigned track, float fxmix, std::uint32_t frames) {
    tracks.at(track & 0xff).fxmix.set(fxmix, frames);
}

void jaudio::SequenceControls::setDolby(unsigned track, float dolby, std::uint32_t frames, unsigned outputMode) {
    auto& parameter = tracks.at(track & 0xff).dolby;
    if (outputMode != 2) {
        dolby = 0;
        if (parameter.value == 0) return;
    }
    parameter.set(dolby, frames);
}

float jaudio::SequenceControls::value(unsigned track, SequenceParameter parameter, unsigned soundState) const {
    if (soundState != 4 && soundState != 5) return -1;
    const auto& control = tracks.at(static_cast<std::int8_t>(track));
    switch (parameter) {
    case SequenceParameter::volume: return control.volume.value;
    case SequenceParameter::pan: return control.pan.value;
    case SequenceParameter::pitch: return control.pitch.value;
    case SequenceParameter::fxmix: return control.fxmix.value;
    case SequenceParameter::dolby: return control.dolby.value;
    }
    throw std::runtime_error("Unsupported original sequence parameter.");
}

void jaudio::SequenceControls::update() {
    for (auto& track : tracks) {
        track.volume.update();
        track.pitch.update();
        track.pan.update();
        track.fxmix.update();
        track.dolby.update();
        track.muted = track.requestedMute;
    }
}

const jaudio::SequenceTrackControl& jaudio::SequenceControls::track(unsigned index) const { return tracks.at(index); }
