#include "jlib/jaudio/jaudiodspmixer.h"

#include <algorithm>
#include <stdexcept>

namespace {
    std::int64_t floorDivide(std::int64_t value, std::int64_t divisor) {
        return value >= 0 ? value / divisor : -1 - (-(value + 1) / divisor);
    }

    std::int16_t saturate(std::int64_t value) {
        return static_cast<std::int16_t>(std::clamp<std::int64_t>(value, -32768, 32767));
    }

    std::int16_t wrap(std::int64_t value) {
        const auto bits = static_cast<std::uint16_t>(value);
        return static_cast<std::int16_t>(bits < 32768 ? static_cast<int>(bits) : static_cast<int>(bits) - 65536);
    }

    std::int64_t roundProduct(std::int64_t value) {
        auto result = floorDivide(value, 32768);
        const auto remainder = value - result * 32768;
        if (remainder > 16384 || (remainder == 16384 && result % 2 != 0)) ++result;
        return result;
    }

    void filter(std::array<std::int16_t, 88>& samples, const std::array<std::int16_t, 8>& coefficients) {
        for (unsigned frame = 0; frame < 80; ++frame) {
            std::int64_t product = 0;
            for (unsigned tap = 0; tap < 8; ++tap) product += static_cast<std::int64_t>(samples[frame + tap]) * coefficients[tap];
            samples[frame] = wrap(roundProduct(product));
        }
    }
}

void jaudio::mixVoice(DspBlock& destination, const DspBlock& source, std::int16_t current, std::int16_t target) {
    for (unsigned frame = 0; frame < source.size(); ++frame) {
        // The DSP multiplies one sample ahead of its 32 pairwise gain updates.
        const auto step = std::min(frame == 0 ? 0U : (frame - 1) / 2, 32U);
        const auto gain = current + floorDivide(static_cast<std::int64_t>(target - current) * step, 32);
        const auto contribution = floorDivide(source[frame] * gain, 65536);
        destination[frame] = saturate(destination[frame] + contribution);
    }
}

std::int16_t jaudio::scaleOutput(std::int16_t sample, std::int16_t gain) {
    return saturate(floorDivide(static_cast<std::int64_t>(sample) * gain, 4096));
}

bool jaudio::EffectProcessor::configure(const EffectConfiguration& configuration, unsigned preset, std::string& error) {
    lines = {};
    error.clear();
    try {
        if (preset >= configuration.presets.size()) throw std::runtime_error("Invalid JAudio effect preset.");
        for (unsigned index = 0; index < lines.size(); ++index) {
            auto& line = lines[index];
            line.configuration = configuration.presets[preset][index];
            for (const auto& destination : line.configuration.destinations) {
                if (destination.bus >= 12) throw std::runtime_error("Invalid JAudio effect destination.");
            }
            if (configuration.allocationBlocks[index] == 0 || line.configuration.flags == 0) continue;
            if (line.configuration.delayBlocks == 0 || line.configuration.delayBlocks >
                static_cast<std::uint64_t>(configuration.allocationBlocks[index]) * 2) {
                throw std::runtime_error("Invalid JAudio effect delay.");
            }
            line.delay.resize(line.configuration.delayBlocks);
        }
        return true;
    } catch (const std::exception& exception) {
        lines = {};
        error = exception.what();
        return false;
    }
}

void jaudio::EffectProcessor::reset() {
    for (auto& line : lines) {
        for (auto& block : line.delay) block.fill(0);
        line.history.fill(0);
        line.position = 0;
    }
}

void jaudio::EffectProcessor::begin(DspBuses& buses) {
    for (unsigned index = 0; index < lines.size(); ++index) {
        auto& line = lines[index];
        if (line.delay.empty()) continue;
        std::array<std::int16_t, 88> samples;
        std::copy(line.history.begin(), line.history.end(), samples.begin());
        const auto& delayed = line.delay[line.position];
        std::copy(delayed.begin(), delayed.end(), samples.begin() + 8);
        std::copy(samples.begin() + 80, samples.end(), line.history.begin());
        if ((line.configuration.flags & 1) != 0) filter(samples, line.configuration.coefficients);
        for (const auto& destination : line.configuration.destinations) {
            if (destination.bus == 11) continue;
            auto& output = buses[destination.bus];
            const auto gain = wrap(destination.gain);
            for (unsigned frame = 0; frame < 80; ++frame) {
                output[frame] = wrap(output[frame] + roundProduct(static_cast<std::int64_t>(samples[frame]) * gain));
            }
        }
        if ((line.configuration.flags & 2) != 0) filter(samples, line.configuration.coefficients);
        std::copy_n(samples.begin(), 80, buses[index + 2].begin());
    }
}

void jaudio::EffectProcessor::end(const DspBuses& buses) {
    for (unsigned index = 0; index < lines.size(); ++index) {
        auto& line = lines[index];
        if (line.delay.empty()) continue;
        line.delay[line.position] = buses[index + 2];
        line.position = (line.position + 1) % line.delay.size();
    }
}

bool jaudio::EffectProcessor::hasTail() const {
    for (const auto& line : lines) {
        for (const auto value : line.history) if (value != 0) return true;
        for (const auto& block : line.delay) {
            for (const auto value : block) if (value != 0) return true;
        }
    }
    return false;
}

bool jaudio::DspMixer::configure(const EffectConfiguration& configuration, std::string& error) {
    buses = {};
    return effects.configure(configuration, 0, error);
}

void jaudio::DspMixer::reset() {
    buses = {};
    effects.reset();
}

void jaudio::DspMixer::begin() {
    for (unsigned frame = 0; frame < 80; ++frame) {
        buses[0][frame] = buses[7][frame];
        buses[1][frame] = wrap(~static_cast<int>(buses[7][frame]));
    }
    for (unsigned bus = 6; bus <= 10; ++bus) buses[bus].fill(0);
    effects.begin(buses);
}

void jaudio::DspMixer::mix(unsigned destination, const DspBlock& source, std::int16_t current, std::int16_t target) {
    if (destination == 0) return;
    if (destination > 11) throw std::runtime_error("Invalid JAudio voice bus destination.");
    if (destination == 10 && (current != 0 || target != 0)) throw std::runtime_error("JAudio chorus bus is not implemented.");
    mixVoice(buses[destination - 1], source, current, target);
}

void jaudio::DspMixer::end(std::array<std::int16_t, 160>& samples, std::int16_t outputLevel) {
    for (const auto value : buses[9]) {
        if (value != 0) throw std::runtime_error("JAudio chorus return is not implemented.");
    }
    for (unsigned frame = 0; frame < 80; ++frame) {
        const auto center = roundProduct(static_cast<std::int64_t>(buses[10][frame]) * 0x5a82);
        const auto left = wrap(buses[0][frame] + center);
        const auto right = wrap(buses[1][frame] + center - buses[8][frame]);
        samples[frame * 2] = scaleOutput(left, outputLevel);
        samples[frame * 2 + 1] = scaleOutput(right, outputLevel);
    }
    effects.end(buses);
}

bool jaudio::DspMixer::hasTail() const {
    for (const auto value : buses[7]) if (value != 0) return true;
    return effects.hasTail();
}
