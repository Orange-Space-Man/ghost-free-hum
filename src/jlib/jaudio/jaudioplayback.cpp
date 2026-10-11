#include "jlib/jaudio/jaudioplayback.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

bool jaudio::Playback::load(gameassets::Package& package, const Resources& resources, std::string& error) {
    effectDispatch = false;
    stop();
    entryPoint = 0;
    tempo = 48;
    timebase = 120;
    if (!readSequence(resources, 0x80000021, sequence, error) || !bank.load(package, resources, error) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto info = soundInfo(resources, 0x80000021);
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
    effectDispatch = false;
    stop();
    entryPoint = 0;
    tempo = 48;
    timebase = 120;
    if (!readSequence(resources, 0x80000028, sequence, error) || !bank.load(package, resources, error) ||
        !mixer.configure(resources.effects, error)) return false;
    const auto info = soundInfo(resources, 0x80000028);
    if (!info) { error = "Missing menu music metadata."; return false; }
    syncFlags = ((*info)[1] << 8) | (*info)[2];
    return true;
}

void jaudio::Playback::start() {
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
    if (event.type == EventType::sync) return;
    for (auto& voice : voices) {
        if (!voice.envelope.released && voice.track == event.track && (voice.slot == event.slot || event.type == EventType::finish)) {
            voice.envelope.release(voice.instrument.envelope, event.type == EventType::finish ? 10 : voice.instrument.release);
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
        if (!interpreter.advance([this](const auto& value) { event(value); }, error)) throw std::runtime_error(error);
        for (auto& voice : voices) {
            if (voice.remaining > 0 && --voice.remaining == 0) voice.envelope.release(voice.instrument.envelope, voice.instrument.release);
            const auto& track = interpreter.track(voice.track);
            float volume = track.parameters[0].value;
            float pitch = bank.pitch(track.parameters[1].value, track.bendRange);
            for (int parent = track.parent; parent >= 0; parent = interpreter.track(parent).parent) {
                volume *= interpreter.track(parent).parameters[0].value;
                pitch *= bank.pitch(interpreter.track(parent).parameters[1].value, interpreter.track(parent).bendRange);
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
            if (spatial.pan != 0.5f) pan += spatial.pan - 0.5f;
            if ((syncFlags & 8) != 0) pan = pan * (1 - 16384.0f / 32767) + 0.5f * (16384.0f / 32767);
            const std::array<float, 3> parameters{std::clamp(pan, 0.0f, 1.0f),
                std::clamp(track.parameters[2].value + spatial.fxmix, 0.0f, 1.0f),
                std::clamp(track.parameters[4].value + spatial.dolby, 0.0f, 1.0f)};
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
std::int16_t jaudio::Playback::outputLevel() const { return static_cast<std::int16_t>(bank.outputLevel); }
bool jaudio::Playback::finished() const { return voicesFinished() && !mixer.hasTail(); }
std::uint64_t jaudio::Playback::notes() const { return noteCount; }
std::uint64_t jaudio::Playback::ticks() const { return interpreter.ticks(); }
unsigned jaudio::Playback::peakVoices() const { return voicePeak; }
