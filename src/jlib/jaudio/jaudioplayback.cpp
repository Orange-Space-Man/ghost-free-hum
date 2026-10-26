#include "jlib/jaudio/jaudioplayback.h"
#include "jlib/jaudio/jaudioeffectparameterpool.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

bool jaudio::Playback::load(gameassets::Package& package, const Resources& resources, std::string& error) {
    return loadMusic(package, resources, 0x80000021, error);
}

bool jaudio::Playback::loadMusic(gameassets::Package& package, const Resources& resources, unsigned id, std::string& error) {
    effectDispatch = false;
    stop();
    entryPoint = 0;
    tempo = 48;
    timebase = 120;
    if (!readSequence(resources, id, sequence, error) || !bank.load(package, resources, error) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto info = soundInfo(resources, id);
    if (!info) { error = "Missing music metadata."; return false; }
    syncFlags = ((*info)[1] << 8) | (*info)[2];
    return true;
}

bool jaudio::Playback::loadStartSound(gameassets::Package& package, const Resources& resources, std::string& error) {
    return loadMenuEffect(package, resources, 0x3802, error);
}

bool jaudio::Playback::loadMenuEffect(gameassets::Package& package, const Resources& resources, unsigned id, std::string& error) {
    effectDispatch = false;
    stop();
    if (id < 0x3801 || id > 0x3803) { error = "Unsupported menu effect ID."; return false; }
    if (!readSequence(resources, 0x80000800, sequence, error) || !bank.load(package, resources, error, 8, 8) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto info = soundInfo(resources, id);
    const std::array<unsigned, 3> dispatch{0x6063,0x6088,0x60b2};
    const auto entry = dispatch[id - 0x3801];
    if (!info || sequence.size() <= entry + 7 || sequence[entry] != 0xc1 || sequence[entry + 1] != 0) {
        error = "Unsupported JAudio Start sound dispatch.";
        return false;
    }
    const auto address = [this](std::size_t offset) -> unsigned {
        return (sequence.at(offset) << 16) | (sequence.at(offset + 1) << 8) | sequence.at(offset + 2);
    };
    if (address(0x5fb0 + (id - 0x3800) * 3) != entry || address(entry + 2) != entry + 7) {
        error = "Unsupported JAudio Start sound entry.";
        return false;
    }
    entryPoint = address(entry + 2);
    tempo = 120;
    timebase = 48;
    syncFlags = ((*info)[1] << 8) | (*info)[2];
    return true;
}

bool jaudio::Playback::loadMenu(gameassets::Package& package, const Resources& resources, std::string& error) {
    return loadMusic(package, resources, 0x80000028, error);
}

bool jaudio::Playback::loadMansion(gameassets::Package& package, const Resources& resources, std::string& error) {
    return loadMusic(package, resources, 0x80000007, error);
}

bool jaudio::Playback::loadDeath(gameassets::Package& package, const Resources& resources, std::string& error) {
    return loadMusic(package, resources, 0x80000012, error);
}

bool jaudio::Playback::loadEffects(gameassets::Package& package, const Resources& resources, std::string& error) {
    effectDispatch = false;
    stop();
    entryPoint = 0;
    tempo = 48;
    timebase = 120;
    if (!readSequence(resources, 0x80000800, sequence, error) || !bank.load(package, resources, error, 0, 0, {1, 2}, {1, 5}) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto* info = soundInfo(resources, 0x80000800);
    if (!info) { error = "Missing effect master metadata."; return false; }
    syncFlags = ((*info)[1] << 8) | (*info)[2];
    return true;
}

void jaudio::Playback::setTrackMuted(unsigned track, bool muted) { controls.setMuted(track, muted); }

void jaudio::Playback::publishEffectParameters(unsigned channel, EffectRecord& record, float categoryVolume, unsigned outputMode) {
    if (!record.parameters) throw std::runtime_error("Effect publication requires an admitted parameter bank.");
    auto& cached = effectParameterCache.at(channel & 255);
    auto& parameters = record.parameters->controls;
    effectOutputMode = outputMode;
    const auto publish = [&](float& previous, float value, auto setter) {
        if (previous == value) return;
        previous = value;
        if (record.state != 2) setter(value);
    };
    publish(cached.volume, parameters.combined(EffectControl::volume, categoryVolume),
        [&](float value) { controls.setVolume(channel, value, 0); if (controls.track(channel & 255).volume.remaining) effectParameterPending.at(channel & 255) |= 1; });
    publish(cached.pan, parameters.combined(EffectControl::pan),
        [&](float value) { controls.setPan(channel, value, 0); if (controls.track(channel & 255).pan.remaining) effectParameterPending.at(channel & 255) |= 2; });
    publish(cached.pitch, parameters.combined(EffectControl::pitch),
        [&](float value) { controls.setPitch(channel, value, 0); if (controls.track(channel & 255).pitch.remaining) effectParameterPending.at(channel & 255) |= 4; });
    publish(cached.fxmix, parameters.combined(EffectControl::fxmix),
        [&](float value) { controls.setFxmix(channel, value, 0); if (controls.track(channel & 255).fxmix.remaining) effectParameterPending.at(channel & 255) |= 8; });
    publish(cached.dolby, parameters.combined(EffectControl::dolby),
        [&](float value) { controls.setDolby(channel, value, 0, outputMode); if (controls.track(channel & 255).dolby.remaining) effectParameterPending.at(channel & 255) |= 16; });
}

const jaudio::SpatialParameters& jaudio::Playback::effectParameters(unsigned channel) const {
    return effectParameterCache.at(channel & 255);
}

bool jaudio::Playback::writeEffectPort(unsigned channel, unsigned port, std::uint16_t value) {
    return interpreter.writeRoutePort(0x20000000 | (channel & 255), port & 255, value);
}

bool jaudio::Playback::readEffectPort(unsigned channel, unsigned port, std::uint16_t& value) const {
    return interpreter.readRoutePort(0x20000000 | (channel & 255), port & 255, value);
}

void jaudio::Playback::publishEffectPort(unsigned channel, unsigned port, std::uint16_t value) {
    channel &= 255;
    port &= 255;
    auto& cached = trackPorts.at(channel).at(port);
    if (cached == value) return;
    writeEffectPort(channel, port, value);
    cached = value;
}

void jaudio::Playback::stopEffectTrack(unsigned channel) {
    channel &= 255;
    writeEffectPort(channel, 0, 0);
    effectStopPending.at(channel) = true;
}
void jaudio::Playback::setTrackVolume(unsigned track, float volume, unsigned frames) { controls.setVolume(track, volume, frames); }
void jaudio::Playback::setTrackPitch(unsigned track, float pitch, unsigned frames) { controls.setPitch(track, pitch, frames); }
void jaudio::Playback::updateSequenceControls() {
    controls.update();
    for (unsigned channel = 0; channel < effectParameterPending.size(); ++channel) {
        if (!effectParameterPending[channel]) continue;
        const auto& control = controls.track(channel);
        interpreter.updateEffectParameters(channel, {control.volume.value, control.pan.value, control.pitch.value,
            control.fxmix.value, control.dolby.value}, effectParameterPending[channel]);
        effectParameterPending[channel] = 0;
    }
    for (unsigned channel = 0; channel < effectStopPending.size(); ++channel) {
        if (!effectStopPending[channel]) continue;
        interpreter.requestRouteInterrupt(0x20000000 | channel, 5);
        effectStopPending[channel] = false;
    }
    if (tempoPending) {
        interpreter.setTempoScale(tempoScale.value);
        tempoScale.update();
        tempoPending = tempoScale.remaining != 0;
    }
}
const jaudio::SequenceControls& jaudio::Playback::sequenceControls() const { return controls; }

void jaudio::Playback::setTempoScale(float value, unsigned frames) {
    if (frames == 0) {
        tempoScale.target = tempoScale.value = value;
        tempoScale.remaining = 0;
    } else tempoScale.set(value, frames);
    tempoPending = true;
}

void jaudio::Playback::setRootPort(unsigned port, std::uint16_t value) { interpreter.writePort(0, port, value); }
std::uint16_t jaudio::Playback::rootPort(unsigned port) const { return static_cast<std::uint16_t>(interpreter.track(0).ports.at(port)); }

void jaudio::Playback::setTrackPort(unsigned track, unsigned port, std::uint16_t value) {
    track &= 255;
    port &= 255;
    if (port >= 16) throw std::out_of_range("Sequence port index exceeds the original port bank.");
    if ((writtenPorts.at(track) & (1u << port)) && trackPorts.at(track).at(port) == value) return;
    trackPorts.at(track).at(port) = value;
    writtenPorts.at(track) |= static_cast<std::uint16_t>(1u << port);
    pendingPorts.at(track) |= static_cast<std::uint16_t>(1u << port);
    for (unsigned i = 1; i < 65; ++i) {
        const auto& current = interpreter.track(i);
        if (current.active && current.route == (0x10000000 | track)) {
            interpreter.writePort(i, port, value);
            pendingPorts[track] &= static_cast<std::uint16_t>(~(1u << port));
        }
    }
}

float jaudio::Playback::externalVolume(const Track& track) const {
    if (track.effectControl) return track.effectParameters[0];
    if (!track.externalControl) return 1;
    const unsigned depth = track.route >> 28;
    const unsigned index = depth == 1 ? track.route & 15 : depth == 2 ? track.route & 255 : 0;
    const auto& control = controls.track(index);
    return control.muted ? 0 : (syncFlags & 1) ? control.volume.value : 1;
}

jaudio::SpatialParameters jaudio::Playback::effectTrackSpatial(unsigned index) const {
    const auto& track = interpreter.track(index);
    SpatialParameters result;
    result.pan = track.parameters[3].value;
    result.fxmix = track.parameters[2].value;
    result.dolby = track.parameters[4].value;
    auto modes = track.externalBlend;
    auto parentModes = track.parentBlend;
    if (track.effectControl || track.externalControl) {
        auto values = track.effectParameters;
        if (!track.effectControl) {
            const auto& control = controls.track((track.route >> 28) == 1 ? track.route & 15 : track.route & 255);
            values = {control.volume.value, control.pan.value, control.pitch.value, control.fxmix.value, control.dolby.value};
        }
        result.pan = blendTrackParameter(result.pan, values[1], track.externalWeight, modes[0]);
        result.fxmix = blendTrackParameter(result.fxmix, values[3], track.externalWeight, modes[1]);
        result.dolby = blendTrackParameter(result.dolby, values[4], track.externalWeight, modes[2]);
    }
    if (track.parent >= 0) {
        const auto parent = effectTrackSpatial(track.parent);
        result.pan = blendTrackParameter(result.pan, parent.pan, track.parentWeight, parentModes[0]);
        result.fxmix = blendTrackParameter(result.fxmix, parent.fxmix, track.parentWeight, parentModes[1]);
        result.dolby = blendTrackParameter(result.dolby, parent.dolby, track.parentWeight, parentModes[2]);
    }
    return result;
}

float jaudio::Playback::externalPitch(const Track& track) const {
    if (track.effectControl) return track.effectParameters[2];
    if (!track.externalControl || !(syncFlags & 2)) return 1;
    const unsigned depth = track.route >> 28;
    const unsigned index = depth == 1 ? track.route & 15 : depth == 2 ? track.route & 255 : 0;
    return controls.track(index).pitch.value;
}

void jaudio::Playback::start() {
    controls = {};
    tempoScale = {};
    tempoPending = false;
    trackPorts = {};
    writtenPorts = {};
    pendingPorts = {};
    effectParameterPending = {};
    effectStopPending = {};
    voices.clear();
    mixer.reset();
    if (effectDispatch) interpreter.startEffect(sequence, entryPoint);
    else interpreter.start(sequence, entryPoint, tempo, timebase);
    noteCount = 0;
    voicePeak = 0;
}

bool jaudio::Playback::loadPositionalEffect(gameassets::Package& package, const Resources& resources,
    unsigned id, std::string& error) {
    stop();
    effectDispatch = false;
    if (id != 0x1809 && id != 0x180a && id != 0x180b && id != 0x181e) {
        error = "Positional sound sequence is outside the recovered first-area door scope.";
        return false;
    }
    if (!readSequence(resources, 0x80000800, sequence, error) || !bank.load(package, resources, error, 0, 0) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto* info = soundInfo(resources, id);
    const auto offset = 0x13e4 + (id & 0x3ff) * 3;
    if (!info || offset + 3 > sequence.size()) { error = "Missing original positional sound dispatch."; return false; }
    entryPoint = (unsigned(sequence[offset]) << 16) | (unsigned(sequence[offset + 1]) << 8) | sequence[offset + 2];
    if (entryPoint >= sequence.size() || sequence[entryPoint] != 0xc1) {
        error = "Unsupported original positional sound entry.";
        return false;
    }
    syncFlags = ((*info)[1] << 8) | (*info)[2];
    effectDispatch = true;
    return true;
}

void jaudio::Playback::stop() {
    voices.clear();
    mixer.reset();
    interpreter.start({});
}

void jaudio::Playback::release() { interpreter.release([this](const auto& value) { event(value); }); }

void jaudio::Playback::event(const SequenceEvent& event) {
    if (event.type == EventType::sync) {
        const auto& track = interpreter.track(event.track);
        if (event.slot == 0 || event.slot == 127) interpreter.configureSequenceBlend(event.track, syncFlags, event.slot == 127);
        if (event.slot == 1) {
            const auto& cached = effectParameterCache.at(track.route & 255);
            interpreter.bindEffectParameters(event.track, {cached.volume, cached.pan, cached.pitch, cached.fxmix,
                effectOutputMode == 2 ? cached.dolby : 0});
        }
        if ((event.slot == 0 || event.slot == 127) && track.externalControl && (track.route >> 28) == 1) {
            const unsigned slot = track.route & 15;
            for (unsigned port = 0; port < 16; ++port) {
                if (pendingPorts[slot] & (1u << port)) interpreter.writePort(event.track, port, trackPorts[slot][port]);
            }
            pendingPorts[slot] = 0;
        }
        return;
    }
    for (auto& voice : voices) {
        if (voice.track == event.track && (voice.slot == event.slot || event.type == EventType::finish)) {
            voice.noteAttached = false;
            if (!voice.envelope.released) voice.envelope.release(voice.instrument.envelope, event.type == EventType::finish ? 10 : voice.instrument.release);
        }
    }
    if (event.type != EventType::noteOn) return;
    const auto& track = interpreter.track(event.track);
    Voice voice;
    voice.track = event.track;
    voice.routes = track.buses;
    voice.slot = event.slot;
    voice.remaining = event.duration;
    voice.instrument = bank.voice(track.bank, track.program, event.key, event.velocity);
    if (event.targetKey >= 0) {
        voice.pitchTarget = bank.keyPitch(voice.instrument, static_cast<unsigned>(event.targetKey));
        voice.pitchRemaining = static_cast<std::uint16_t>(event.duration);
        if (!event.duration) voice.instrument.pitch = voice.pitchTarget;
    }
    if (track.adsr && track.envelopeRoute == 0) {
        const auto& values = *track.adsr;
        auto& envelope = voice.instrument.envelope;
        envelope = Oscillator{};
        envelope.attack = {{0, values[0], 32767}, {0, values[1], 32767},
            {0, values[2], static_cast<std::int16_t>(values[3])}, {14, 0, 0}};
        envelope.release = {{0, values[4], 0}, {15, 1, 0}};
    }
    voice.hasVibrato = track.vibrato && track.oscillatorRoute == 0x11;
    voice.vibratoDefinition = bank.vibrato;
    voice.envelope.advance(voice.instrument.envelope);
    if (voice.hasVibrato) voice.vibrato.advance(voice.vibratoDefinition);
    voices.push_back(std::move(voice));
    if (voices.size() > 256) throw std::runtime_error("JAudio voice capacity exceeded.");
    ++noteCount;
    voicePeak = std::max(voicePeak, static_cast<unsigned>(voices.size()));
}

bool jaudio::Playback::render(std::array<std::int16_t, 160>& samples, std::string& error) {
    samples.fill(0);
    mixer.begin();
    if (!mix(mixer, error)) return false;
    try {
        mixer.end(samples, outputLevel());
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        stop();
        return false;
    }
}

bool jaudio::Playback::mix(DspMixer& destination, std::string& error, const SpatialParameters& spatial) {
    error.clear();
    try {
        interpreter.noteStopped = [this](unsigned track, unsigned slot) {
            return std::none_of(voices.begin(), voices.end(), [&](const auto& voice) {
                return voice.track == track && voice.slot == slot && voice.noteAttached && !voice.ended;
            });
        };
        if (!interpreter.advance([this](const auto& value) { event(value); }, error)) throw std::runtime_error(error);
        for (auto& voice : voices) {
            if (voice.pitchRemaining) {
                voice.instrument.pitch += (voice.pitchTarget - voice.instrument.pitch) / voice.pitchRemaining;
                --voice.pitchRemaining;
            }
            if (voice.remaining > 0 && --voice.remaining == 0) voice.envelope.release(voice.instrument.envelope, voice.instrument.release);
            const auto& track = interpreter.track(voice.track);
            float volume = track.parameters[0].value * externalVolume(track);
            float pitch = bank.pitch(track.parameters[1].value, track.bendRange) * externalPitch(track);
            for (int parent = track.parent; parent >= 0; parent = interpreter.track(parent).parent) {
                volume *= interpreter.track(parent).parameters[0].value * externalVolume(interpreter.track(parent));
                pitch *= bank.pitch(interpreter.track(parent).parameters[1].value, interpreter.track(parent).bendRange) * externalPitch(interpreter.track(parent));
            }
            volume *= voice.instrument.volume * voice.envelope.advance(voice.instrument.envelope) * spatial.volume;
            if (voice.envelope.ended) { voice.ended = true; continue; }
            if (voice.hasVibrato) {
                auto& oscillator = voice.vibratoDefinition;
                if (track.parameterWritten[9]) oscillator.scale = track.parameters[9].value;
                if (track.parameterWritten[10]) oscillator.rate = track.parameters[10].value;
                if (track.parameterWritten[11]) oscillator.offset = track.parameters[11].value;
                pitch *= voice.vibrato.advance(oscillator);
            }
            pitch *= voice.instrument.pitch * spatial.pitch;
            if (!std::isfinite(pitch) || pitch < 0 || pitch >= 16) throw std::runtime_error("Invalid JAudio voice pitch.");
            const auto ratio = static_cast<unsigned>(pitch * 4096);
            auto pan = track.parameters[3].value;
            auto fxmix = track.parameters[2].value;
            auto dolby = track.parameters[4].value;
            bool effectBound = false;
            for (int index = static_cast<int>(voice.track); index >= 0; index = interpreter.track(index).parent) {
                effectBound |= interpreter.track(index).effectControl;
            }
            if (effectBound) {
                const auto inherited = effectTrackSpatial(voice.track);
                pan = inherited.pan;
                fxmix = inherited.fxmix;
                dolby = inherited.dolby;
            }
            if (spatial.pan != 0.5f) pan += spatial.pan - 0.5f;
            if (!effectBound && (syncFlags & 8) != 0) pan = pan * (1 - 16384.0f / 32767) + 0.5f * (16384.0f / 32767);
            const std::array<float, 3> parameters{std::clamp(pan, 0.0f, 1.0f),
                std::clamp(fxmix + spatial.fxmix, 0.0f, 1.0f),
                std::clamp(dolby + spatial.dolby, 0.0f, 1.0f)};
            std::array<std::int16_t, 6> gains{};
            for (unsigned bus = 0; bus < gains.size(); ++bus) {
                const auto route = voice.routes[bus];
                if ((route >> 8) == 0) continue;
                auto level = volume;
                for (unsigned shift : {4U, 0U}) {
                    const auto selector = (route >> shift) & 15;
                    if (selector == 0) continue;
                    if (selector == 4 || selector > 7) throw std::runtime_error("Unsupported JAudio bus gain selector.");
                    const auto value = parameters[(selector - 1) & 3];
                    level *= bank.pan(selector >= 5 ? 1 - value : value);
                }
                gains[bus] = static_cast<std::int16_t>(std::clamp(level, 0.0f, 1.0f) * bank.channelLevel);
            }
            if (voice.firstBlock) {
                voice.gains = gains;
                voice.firstBlock = false;
            }
            DspBlock source{};
            for (unsigned frame = 0; frame < 80; ++frame) {
                const auto& wave = *voice.instrument.wave;
                if ((voice.position >> 12) >= wave.samples.size() + 4 && wave.loop.empty()) { voice.ended = true; break; }
                source[frame] = bank.resample(wave, voice.position, ratio);
                voice.position += ratio;
            }
            for (unsigned bus = 0; bus < gains.size(); ++bus) {
                destination.mix(voice.routes[bus] >> 8, source, voice.gains[bus], gains[bus]);
            }
            voice.gains = gains;
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](const auto& voice) { return voice.ended; }), voices.end());
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        stop();
        return false;
    }
}

bool jaudio::Playback::voicesFinished() const { return interpreter.finished() && voices.empty(); }
bool jaudio::Playback::hasVoices() const { return !voices.empty(); }
std::int16_t jaudio::Playback::outputLevel() const { return static_cast<std::int16_t>(bank.outputLevel); }
bool jaudio::Playback::finished() const { return voicesFinished() && !mixer.hasTail(); }
std::uint64_t jaudio::Playback::notes() const { return noteCount; }
std::uint64_t jaudio::Playback::ticks() const { return interpreter.ticks(); }
unsigned jaudio::Playback::peakVoices() const { return voicePeak; }
