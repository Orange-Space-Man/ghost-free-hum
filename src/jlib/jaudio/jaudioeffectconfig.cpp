#include "jlib/jaudio/jaudioeffectconfig.h"
#include "jlib/jaudio/jaudioeffectparameter.h"

#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Truncated JAudio effect configuration.");
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    std::uint32_t dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }
}

bool jaudio::decodeEffectConfiguration(const std::vector<std::uint8_t>& data, EffectConfiguration& configuration, std::string& error) {
    configuration = {};
    error.clear();
    try {
        EffectConfiguration result;
        const auto count = dword(data, 0);
        require(count > 0 && count <= 255, "Invalid JAudio effect preset count.");
        range(data, 4, 16 + count * 4);
        for (unsigned line = 0; line < 4; ++line) {
            result.allocationBlocks[line] = dword(data, 4 + line * 4);
        }
        result.presets.resize(count);
        for (unsigned preset = 0; preset < count; ++preset) {
            const auto offset = dword(data, 20 + preset * 4);
            require(offset >= 20 + count * 4, "JAudio effect preset overlaps its directory.");
            range(data, offset, 128);
            for (unsigned line = 0; line < 4; ++line) {
                const auto record = static_cast<std::size_t>(offset) + line * 32;
                auto& output = result.presets[preset][line];
                output.flags = data[record];
                const auto delay = dword(data, record + 12);
                require(delay <= 65535, "JAudio effect delay exceeds its DSP field.");
                require(result.allocationBlocks[line] == 0 ||
                    delay <= static_cast<std::uint64_t>(result.allocationBlocks[line]) * 2,
                    "JAudio effect delay exceeds its allocation.");
                require(output.flags == 0 || result.allocationBlocks[line] == 0 || delay != 0,
                    "Enabled JAudio effect has an empty delay.");
                output.delayBlocks = static_cast<std::uint16_t>(delay);
                for (unsigned destination = 0; destination < 2; ++destination) {
                    auto& route = output.destinations[destination];
                    route.bus = static_cast<std::uint16_t>(word(data, record + 2 + destination * 4));
                    route.gain = static_cast<std::uint16_t>(word(data, record + 4 + destination * 4));
                    require(route.bus < 12, "Invalid JAudio effect destination.");
                }
                for (unsigned tap = 0; tap < 8; ++tap) {
                    const auto value = word(data, record + 16 + tap * 2);
                    output.coefficients[tap] = static_cast<std::int16_t>(value < 32768 ? static_cast<int>(value) : static_cast<int>(value) - 65536);
                }
            }
        }
        configuration = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}


void jaudio::EffectParameter::set(float value, std::uint32_t frames) {
    target = value;
    if (current != target) {
        if (!frames) current = target;
        else {
            remaining = frames;
            step = (target - current) / float(frames);
        }
    }
}

void jaudio::EffectParameter::advance() {
    if (remaining) {
        --remaining;
        if (!remaining) current = target;
        else current += step;
    }
}
