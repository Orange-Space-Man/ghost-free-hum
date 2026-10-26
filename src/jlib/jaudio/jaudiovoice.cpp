#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "jlib/jaudio/jaudiovoice.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 2, "Truncated JAudio instrument data.");
        return (data[offset] << 8) | data[offset + 1];
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float real(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        require(std::isfinite(value), "Invalid JAudio instrument parameter.");
        return value;
    }

    std::vector<jaudio::EnvelopePoint> envelope(const std::vector<std::uint8_t>& data, std::size_t offset) {
        std::vector<jaudio::EnvelopePoint> points;
        if (offset == 0) return points;
        for (unsigned index = 0; index < 1024; ++index) {
            const auto curve = word(data, offset);
            points.push_back({curve, word(data, offset + 2), static_cast<std::int16_t>(word(data, offset + 4))});
            if (curve == 13 || curve == 14 || curve == 15) return points;
            require(curve <= 2, "Unported JAudio envelope curve.");
            offset += 6;
        }
        throw std::runtime_error("Unterminated JAudio envelope.");
    }

    jaudio::Oscillator oscillator(const std::vector<std::uint8_t>& data, std::size_t offset) {
        jaudio::Oscillator result;
        if (offset == 0) return result;
        result.destination = dword(data, offset) >> 24;
        result.rate = real(data, offset + 4);
        result.attack = envelope(data, dword(data, offset + 8));
        result.release = envelope(data, dword(data, offset + 12));
        result.scale = real(data, offset + 16);
        result.offset = real(data, offset + 20);
        return result;
    }

    std::vector<std::uint8_t> block(const jaudio::Resources& resources, unsigned kind, unsigned id) {
        for (const auto& entry : resources.sections) {
            if (entry.kind == kind && dword(resources.initialization, entry.offset + 8) == id) {
                return {resources.initialization.begin() + entry.offset, resources.initialization.begin() + entry.offset + entry.size};
            }
        }
        throw std::runtime_error("Missing JAudio title bank.");
    }
}

bool jaudio::decodeAfc(const std::vector<std::uint8_t>& data, unsigned count,
    const std::array<std::int16_t, 32>& coefficients, std::int16_t history1, std::int16_t history2,
    std::vector<std::int16_t>& samples, std::string& error) {
    samples.clear();
    error.clear();
    if ((static_cast<std::uint64_t>(count) + 15) / 16 * 9 > data.size()) {
        error = "Truncated JAudio AFC sample.";
        return false;
    }
    samples.reserve(count);
    for (unsigned index = 0; index < count; ++index) {
        const auto offset = static_cast<std::size_t>(index / 16) * 9;
        const unsigned predictor = data[offset] & 15;
        const int scale = 1 << (data[offset] >> 4);
        const auto packed = data[offset + 1 + (index % 16) / 2];
        int residual = (index & 1) == 0 ? packed >> 4 : packed & 15;
        if (residual >= 8) residual -= 16;
        const auto prediction = static_cast<std::int64_t>(history1) * coefficients[predictor * 2] +
            static_cast<std::int64_t>(history2) * coefficients[predictor * 2 + 1];
        const auto accumulator = static_cast<std::int64_t>(residual) * scale * 2048 + prediction;
        const auto decoded = accumulator >= 0 ? accumulator / 2048 : -((-accumulator + 2047) / 2048);
        const auto value = static_cast<std::int16_t>(std::clamp<std::int64_t>(decoded, -32768, 32767));
        samples.push_back(value);
        history2 = history1;
        history1 = value;
    }
    return true;
}

void jaudio::Envelope::release(const Oscillator& oscillator, unsigned duration) {
    if (released || ended) return;
    released = true;
    holding = false;
    position = 0;
    remaining = 0;
    target = value;
    if (oscillator.release.empty()) {
        fallbackRelease = true;
        curve = duration >> 14;
        remaining = std::max(1.0f, (duration & 0x3fff) * ((32028.5f / 80) / 600));
        target = 0;
        step = -value / remaining;
    }
}

float jaudio::Envelope::advance(const Oscillator& oscillator) {
    if (ended) return 0;
    const auto& points = released ? oscillator.release : oscillator.attack;
    if (points.empty() && !fallbackRelease) { value = 1; return 1; }
    if (holding) return value * oscillator.scale + oscillator.offset;
    remaining -= oscillator.rate;
    for (unsigned transitions = 0; remaining <= 0; ++transitions) {
        require(transitions < 1024, "JAudio envelope did not advance.");
        value = target;
        if (fallbackRelease) { ended = true; return 0; }
        require(position < points.size(), "JAudio envelope read outside its table.");
        const auto& point = points[position++];
        if (point.curve == 13) { position = static_cast<unsigned>(point.target); continue; }
        if (point.curve == 14) { holding = true; return value * oscillator.scale + oscillator.offset; }
        if (point.curve == 15) { ended = true; return value * oscillator.scale + oscillator.offset; }
        curve = point.curve;
        target = point.target / 32768.0f;
        if (point.duration == 0) continue;
        remaining = point.duration * ((32028.5f / 80) / 600);
        step = (target - value) / remaining;
    }
    value = target - step * remaining;
    const auto shaped = curve == 1 ? std::copysign(value * value, value) : curve == 2 ? std::copysign(std::sqrt(std::abs(value)), value) : value;
    return shaped * oscillator.scale + oscillator.offset;
}

void jaudio::VoiceBank::loadWaveSystem(gameassets::Package& package, const Resources& resources, unsigned id) {
    auto& waveData = waveSystems[id];
    waveData = block(resources, 3, id);
    std::string error;
    const auto info = dword(waveData, 16);
    const auto control = dword(waveData, 20);
    const auto groups = dword(waveData, info + 4);
    require(dword(waveData, info) == 0x57494e46 && dword(waveData, control) == 0x57424354 &&
        groups == dword(waveData, control + 8) && groups <= 256, "Invalid JAudio wave directory.");
    for (unsigned group = 0; group < groups; ++group) {
        const auto archive = dword(waveData, info + 8 + group * 4);
        word(waveData, archive + 112);
        const auto first = waveData.begin() + archive;
        const auto last = std::find(first, first + 112, 0);
        require(last != first + 112 && last != first, "Invalid JAudio wave archive name.");
        const std::string filename(first, last);
        require(filename.find_first_of("/\\:") == std::string::npos && filename != "." && filename != "..", "Invalid JAudio wave archive path.");
        if (!package.read("files/AudioRes/Banks/" + filename, archives[filename], error)) throw std::runtime_error(error);
        const auto scene = dword(waveData, control + 12 + group * 4);
        require(dword(waveData, scene) == 0x53434e45, "Invalid JAudio wave scene.");
        unsigned index = 0;
        for (unsigned field : {12U, 16U}) {
            const auto table = dword(waveData, scene + field);
            if (table == 0) continue;
            const auto count = dword(waveData, table + 4);
            require(count <= dword(waveData, archive + 112) - index, "Invalid JAudio wave count.");
            for (unsigned item = 0; item < count; ++item, ++index) {
                const auto entry = dword(waveData, table + 8 + item * 4);
                const auto sampleId = (id << 16) | (dword(waveData, entry) & 0xffff);
                require(locations.emplace(sampleId, WaveLocation{filename, dword(waveData, archive + 116 + index * 4)}).second,
                    "Duplicate JAudio wave ID.");
            }
        }
        require(index == dword(waveData, archive + 112), "Unmapped JAudio wave samples.");
    }
}

bool jaudio::VoiceBank::load(gameassets::Package& package, const Resources& resources, std::string& error, unsigned instrumentBank,
    unsigned waveSystem, const std::vector<unsigned>& additionalWaveSystems, const std::vector<unsigned>& additionalInstrumentBanks) {
    *this = {};
    error.clear();
    try {
        waveId = waveSystem;
        instrumentBanks.emplace(instrumentBank, block(resources, 2, instrumentBank));
        for (const auto id : additionalInstrumentBanks) if (!instrumentBanks.count(id))
            instrumentBanks.emplace(id, block(resources, 2, id));
        loadWaveSystem(package, resources, waveId);
        for (const auto id : additionalWaveSystems) if (waveSystems.count(id) == 0) loadWaveSystem(package, resources, id);
        std::vector<std::uint8_t> dol, table;
        if (!package.read("sys/main.dol", dol, error) ||
            !dolresources::read(dol, glme01::voicePredictorCoefficients, 64, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < coefficients.size(); ++i) coefficients[i] = static_cast<std::int16_t>(word(table, i * 2));
        if (!dolresources::read(dol, glme01::dspResamplingTable, 512, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < resampling.size(); ++i) resampling[i] = static_cast<std::int16_t>(word(table, i * 2));
        if (!dolresources::read(dol, glme01::voiceChannelAndOutputLevels, 8, table, error)) throw std::runtime_error(error);
        channelLevel = static_cast<unsigned>(16384 * real(table, 0));
        outputLevel = static_cast<unsigned>(4096 * real(table, 4));
        if (!dolresources::read(dol, glme01::voiceSineConstants, 8, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < sine.size(); ++i) sine[i] = std::sin((real(table, 0) * i) * real(table, 4));
        if (!dolresources::read(dol, glme01::voicePitchTable, 128 * 4, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < pitches.size(); ++i) pitches[i] = real(table, i * 4);
        if (!dolresources::read(dol, glme01::voiceFinePitchTable, 64 * 4, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < finePitches.size(); ++i) finePitches[i] = real(table, i * 4);
        if (!dolresources::read(dol, glme01::voiceVibratoParameters, 0x6c, table, error)) throw std::runtime_error(error);
        vibrato.destination = 1;
        vibrato.rate = real(table, 0x58);
        vibrato.scale = real(table, 0x64);
        vibrato.offset = real(table, 0x68);
        std::vector<std::uint8_t> relocated(4, 0);
        relocated.insert(relocated.end(), table.begin(), table.end());
        vibrato.attack = envelope(relocated, 4);
        vibrato.release = vibrato.attack;
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        *this = {};
        return false;
    }
}

std::shared_ptr<const jaudio::Wave> jaudio::VoiceBank::wave(unsigned id) {
    if ((id >> 16) == 0xffff) id = (waveId << 16) | (id & 0xffff);
    require(waveSystems.count(id >> 16) != 0, "Unloaded JAudio wave system.");
    const auto& waveData = waveSystems.at(id >> 16);
    if (waves.count(id) != 0) return waves.at(id);
    const auto found = locations.find(id);
    require(found != locations.end(), "Missing JAudio instrument sample.");
    const auto header = found->second.header;
    const auto info = dword(waveData, header);
    require(((info >> 16) & 255) == 0, "Unported JAudio sample encoding.");
    auto result = std::make_shared<Wave>();
    result->rootKey = (info >> 8) & 255;
    result->sampleRate = real(waveData, header + 4);
    const auto offset = dword(waveData, header + 8);
    const auto size = dword(waveData, header + 12);
    const auto& archive = archives.at(found->second.archive);
    require(offset <= archive.size() && size <= archive.size() - offset, "Truncated JAudio wave archive.");
    const std::vector<std::uint8_t> encoded(archive.begin() + offset, archive.begin() + offset + size);
    const auto count = dword(waveData, header + 28);
    const auto looping = dword(waveData, header + 16) != 0;
    const auto end = looping ? dword(waveData, header + 24) : count;
    require(end > 0 && end <= count, "Invalid JAudio sample length.");
    std::string error;
    if (!decodeAfc(encoded, end, coefficients, 0, 0, result->samples, error)) throw std::runtime_error(error);
    if (looping) {
        const auto start = dword(waveData, header + 20);
        require(start < end, "Invalid JAudio sample loop.");
        const auto blockStart = start / 16 * 16;
        const auto byteStart = static_cast<std::size_t>(start / 16) * 9;
        require(byteStart < encoded.size(), "Invalid JAudio loop address.");
        const std::vector<std::uint8_t> loopBytes(encoded.begin() + byteStart, encoded.end());
        if (!decodeAfc(loopBytes, end - blockStart, coefficients, static_cast<std::int16_t>(word(waveData, header + 32)),
            static_cast<std::int16_t>(word(waveData, header + 34)), result->loop, error)) throw std::runtime_error(error);
        result->loop.erase(result->loop.begin(), result->loop.begin() + (start - blockStart));
    }
    waves.emplace(id, result);
    return result;
}

jaudio::InstrumentVoice jaudio::VoiceBank::voice(unsigned bank, unsigned program, unsigned key, unsigned velocity) {
    const auto found = instrumentBanks.find(bank);
    if (found == instrumentBanks.end() || !(program < 128 || (program >= 228 && program < 240)) || key >= 128 || velocity >= 128)
        throw std::runtime_error("Unsupported JAudio voice: bank " + std::to_string(bank) + ", program " + std::to_string(program) +
            ", key " + std::to_string(key) + ", velocity " + std::to_string(velocity) + ".");
    const auto& bankData = found->second;
    const auto instrument = dword(bankData, 0x24 + program * 4);
    require(instrument != 0, "Missing JAudio instrument.");
    InstrumentVoice result;
    unsigned map = 0;
    unsigned count = 0;
    const bool percussion = program >= 228;
    if (percussion) {
        require(dword(bankData, instrument) == 0x50455232, "Unported JAudio percussion bank.");
        const auto percussionMap = dword(bankData, instrument + 0x88 + key * 4);
        require(percussionMap != 0, "Missing JAudio percussion key.");
        result.volume = real(bankData, percussionMap);
        result.pitch = real(bankData, percussionMap + 4);
        require(dword(bankData, percussionMap + 8) == 0 && dword(bankData, percussionMap + 12) == 0, "Unported JAudio random percussion effect.");
        count = dword(bankData, percussionMap + 16);
        map = percussionMap + 20;
        word(bankData, instrument + 0x288 + key);
        result.pan = static_cast<std::int8_t>(bankData[instrument + 0x288 + key]) / 127.0f;
        result.release = word(bankData, instrument + 0x308 + key * 2);
    } else {
        require(dword(bankData, instrument) == 0x494e5354 && dword(bankData, instrument + 4) == 0, "Unported JAudio instrument flags.");
        result.volume = real(bankData, instrument + 8);
        result.pitch = real(bankData, instrument + 12);
        result.envelope = oscillator(bankData, dword(bankData, instrument + 16));
        require(result.envelope.destination == 0 && dword(bankData, instrument + 20) == 0, "Unported JAudio instrument oscillator.");
        for (unsigned i = 24; i < 40; i += 4) require(dword(bankData, instrument + i) == 0, "Unported JAudio instrument effect.");
        const auto keys = dword(bankData, instrument + 40);
        require(keys <= 128, "Invalid JAudio key map count.");
        for (unsigned i = 0; i < keys; ++i) {
            const auto keymap = dword(bankData, instrument + 44 + i * 4);
            if ((dword(bankData, keymap) >> 24) >= key) {
                count = dword(bankData, keymap + 4);
                map = keymap + 8;
                break;
            }
        }
    }
    require(count <= 128, "Invalid JAudio velocity map count.");
    for (unsigned i = 0; i < count; ++i) {
        const auto entry = dword(bankData, map + i * 4);
        if ((dword(bankData, entry) >> 24) < velocity) continue;
        result.wave = wave(dword(bankData, entry + 4));
        const auto gain = velocity / 127.0f;
        result.volume *= real(bankData, entry + 8) * gain * gain;
        result.pitch *= real(bankData, entry + 12) * (result.wave->sampleRate / 32028.5f);
        result.basePitch = result.pitch;
        result.percussion = percussion;
        if (!percussion) result.pitch *= pitches[std::clamp(static_cast<int>(key) + 60 - static_cast<int>(result.wave->rootKey), 0, 127)];
        return result;
    }
    throw std::runtime_error("Missing JAudio velocity map.");
}

float jaudio::VoiceBank::keyPitch(const InstrumentVoice& voice, unsigned key) const {
    const int pitchKey = voice.percussion ? static_cast<int>(key & 255) : static_cast<int>(key & 255) + 60 - static_cast<int>(voice.wave->rootKey);
    return voice.basePitch * pitches[std::clamp(pitchKey, 0, 127)];
}

float jaudio::VoiceBank::pitch(float bend, unsigned range) const {
    const auto semitones = bend * 4 * range;
    auto whole = static_cast<int>(semitones);
    auto remainder = semitones - whole;
    if (semitones < 0 && remainder != 0) {
        --whole;
        remainder += 1;
    }
    if (remainder >= 1) {
        remainder -= 1;
        ++whole;
    }
    require(whole >= -60 && whole < 68, "JAudio pitch bend outside the original table.");
    const auto fraction = static_cast<unsigned>(remainder * 64);
    return pitches[60 + whole] * finePitches[fraction];
}

float jaudio::VoiceBank::pan(float value) const {
    return sine[static_cast<unsigned>(std::clamp(value, 0.0f, 1.0f) * 256)];
}

std::int16_t jaudio::VoiceBank::resample(const Wave& wave, std::uint64_t position, unsigned ratio) const {
    return resampleWave(wave, position, ratio, resampling);
}

std::int16_t jaudio::resampleWave(const Wave& wave, std::uint64_t position, unsigned ratio,
    const std::array<std::int16_t, 256>& coefficients) {
    const auto sample = [&wave](std::int64_t index) -> std::int16_t {
        if (index < 0) return 0;
        const auto offset = static_cast<std::uint64_t>(index);
        if (offset < wave.samples.size()) return wave.samples[static_cast<std::size_t>(offset)];
        if (wave.loop.empty()) return 0;
        return wave.loop[static_cast<std::size_t>((offset - wave.samples.size()) % wave.loop.size())];
    };
    if (ratio >= 0x4000) return sample(static_cast<std::int64_t>((position + ratio) >> 12) - 4);
    const auto index = static_cast<std::int64_t>(position >> 12) - 4;
    const auto phase = static_cast<unsigned>((position & 4095) / 64) * 4;
    std::int64_t sum = 0;
    for (unsigned tap = 0; tap < 4; ++tap) sum += static_cast<std::int64_t>(sample(index + tap)) * coefficients[phase + tap];
    auto value = sum >= 0 ? sum / 32768 : -((-sum + 32767) / 32768);
    const auto remainder = sum - value * 32768;
    if (remainder > 16384 || (remainder == 16384 && value % 2 != 0)) ++value;
    return static_cast<std::int16_t>(std::clamp<std::int64_t>(value, -32768, 32767));
}
