#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "jlib/jaudio/jaudiostream.h"
#include "jlib/jaudio/jaudioresources.h"
#include "jlib/jaudio/jaudiovoice.h"
#include "jlib/jaudio/jaudiodspmixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) throw std::runtime_error("Invalid stream mixer parameter.");
        return value;
    }
}

bool jaudio::decodeStream(const std::vector<std::uint8_t>& data, const std::array<std::int16_t, 32>& coefficients,
    StreamAudio& audio, std::string& error) {
    error.clear();
    try {
        if (data.size() < 32 || word(data, 10) != 4 || word(data, 12) != 16)
            throw std::runtime_error("Unsupported JAudio stream header.");
        const auto count = dword(data, 4);
        const std::uint64_t blocks = (std::uint64_t(count) + 15) / 16;
        if (count == 0 || blocks * 18 > data.size() - 32 || dword(data, 0) != blocks * 18)
            throw std::runtime_error("Truncated JAudio stream.");
        StreamAudio result;
        result.sampleRate = word(data, 8);
        result.frameRate = word(data, 14);
        if (!result.sampleRate || !result.frameRate) throw std::runtime_error("Invalid JAudio stream rate.");
        result.samples.resize(std::size_t(count) * 2);
        std::array<std::int16_t, 2> history1{}, history2{};
        for (std::size_t block = 0; block < blocks; ++block) for (unsigned channel = 0; channel < 2; ++channel) {
            const auto offset = 32 + block * 18 + channel * 9;
            const auto predictor = data[offset] & 15;
            const int scale = 1 << (data[offset] >> 4);
            for (unsigned sample = 0; sample < 16 && block * 16 + sample < count; ++sample) {
                const auto packed = data[offset + 1 + sample / 2];
                int residual = (sample & 1) ? (packed & 15) : (packed >> 4);
                if (residual >= 8) residual -= 16;
                const std::int64_t prediction = std::int64_t(coefficients[predictor * 2]) * history1[channel] +
                    std::int64_t(coefficients[predictor * 2 + 1]) * history2[channel];
                const auto rounded = prediction >= 0 ? prediction / 2048 : -((-prediction + 2047) / 2048);
                const auto value = static_cast<std::int16_t>(residual * scale + rounded);
                result.samples[(block * 16 + sample) * 2 + channel] = value;
                history2[channel] = history1[channel];
                history1[channel] = value;
            }
        }
        audio = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool jaudio::readStream(gameassets::Package& package, const std::string& path, StreamAudio& audio, std::string& error) {
    try {
        std::vector<std::uint8_t> data, executable, table;
        if (!package.read(path, data, error) || !package.read("sys/main.dol", executable, error) ||
            !dolresources::read(executable, glme01::streamPredictorCoefficients, 64, table, error)) return false;
        std::array<std::int16_t, 32> coefficients{};
        for (unsigned i = 0; i < coefficients.size(); ++i) coefficients[i] = static_cast<std::int16_t>(word(table, i * 2));
        StreamAudio result;
        if (!decodeStream(data, coefficients, result, error)) return false;
        Resources resources;
        std::vector<std::uint8_t> initialization;
        if (!package.read("files/AudioRes/JaiInit.aaf", initialization, error) ||
            !decodeInitialization(initialization, resources, error)) return false;
        const unsigned id = path == "files/AudioRes/Stream/TMansion.afc" ? 0xc0000000 :
            path == "files/AudioRes/Stream/TMOpen.afc" ? 0xc0000001 : 0;
        const auto info = soundInfo(resources, id);
        if (!id || !info) throw std::runtime_error("Unsupported opening stream metadata.");
        const std::vector<std::uint8_t> metadata(info->begin(), info->end());
        unsigned flags = 0;
        for (const auto& section : resources.sections) if (section.kind == 1) flags = initialization.at(section.offset + 2);
        if (!dolresources::read(executable, glme01::streamVolumeDivisor, 4, table, error)) return false;
        const float volume = (flags & 4) ? metadata[12] / number(table, 0) : 1;
        const float pitch = (flags & 2) ? number(metadata, 8) : 1;
        if (volume < 0 || volume > 1 || pitch <= 0 || pitch > 4) throw std::runtime_error("Invalid opening stream gain or pitch.");
        const auto ratio = static_cast<unsigned>(((result.sampleRate << 12) / 32000) * pitch);
        if (!ratio || ratio >= 0x4000) throw std::runtime_error("Unsupported opening stream resampling ratio.");
        const auto gain = static_cast<std::int16_t>(32767 * volume);
        if (!dolresources::read(executable, glme01::streamOutputLevel, 4, table, error)) return false;
        const auto outputLevel = static_cast<std::int16_t>(4096 * number(table, 0));
        if (!dolresources::read(executable, glme01::dspResamplingTable, 512, table, error)) return false;
        std::array<std::int16_t, 256> resampling{};
        for (unsigned i = 0; i < resampling.size(); ++i) resampling[i] = static_cast<std::int16_t>(word(table, i * 2));
        std::array<Wave, 2> channels;
        const auto count = result.samples.size() / 2;
        for (auto& channel : channels) channel.samples.resize(count);
        for (std::size_t frame = 0; frame < count; ++frame)
            for (unsigned channel = 0; channel < 2; ++channel) channels[channel].samples[frame] = result.samples[frame * 2 + channel];
        result.framesPerSample = double(ratio) * result.frameRate / (4096.0 * result.sampleRate);
        result.sampleRate = 32000;
        result.playbackRate = 32028.5;
        result.samples.clear();
        DspMixer mixer;
        if (!mixer.configure(resources.effects, error)) return false;
        std::uint64_t position = 0;
        while ((position >> 12) < count) {
            mixer.begin();
            for (unsigned channel = 0; channel < 2; ++channel) {
                DspBlock samples{};
                for (unsigned frame = 0; frame < 80; ++frame)
                    samples[frame] = resampleWave(channels[channel], position + std::uint64_t(frame) * ratio, ratio, resampling);
                mixer.mix(channel + 1, samples, position <= 80ULL * ratio ? 32767 : gain, position == 0 ? 32767 : gain);
                if (position == 0) mixer.mix(2 - channel, samples, 32767, 0);
            }
            std::array<std::int16_t, 160> output{};
            mixer.end(output, outputLevel);
            result.samples.insert(result.samples.end(), output.begin(), output.end());
            position += 80ULL * ratio;
        }
        audio = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
