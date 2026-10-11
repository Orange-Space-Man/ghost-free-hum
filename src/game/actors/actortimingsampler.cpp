#include "game/actors/actortimingsampler.h"
#include "game/actors/actorpathsampler.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (half(bytes, offset) << 16) | half(bytes, offset + 2);
    }
}

float actoranimation::sampleTiming(const actormodel::Resource* resource, std::string_view name, double frame) {
    if (!resource) return -10000;
    const auto& bytes = resource->bytes;
    const auto count = half(bytes, 0), records = word(bytes, 4);
    if (records > bytes.size() || std::size_t(count) * 36 > bytes.size() - records)
        throw std::runtime_error("Original actor timing records exceed their resource.");
    name = name.substr(0, name.find('\0'));
    for (unsigned i = 0; i < count; ++i) {
        const auto offset = records + i * 36;
        std::string_view label(reinterpret_cast<const char*>(bytes.data() + offset), 30);
        label = label.substr(0, label.find('\0'));
        if (label.find(name) != std::string_view::npos) return sampleFloatTrack(bytes, offset + 30, 8, frame);
    }
    return -10000;
}

std::uint8_t actoranimation::timingAlpha(float value) {
    if (value == -10000) return 255;
    float clamped = 0;
    if (value >= 0) clamped = value > 1 ? 1 : value;
    const double scaled = 255.0 * double(clamped);
    const auto integer = !std::isfinite(scaled) || scaled < -2147483648.0 || scaled >= 2147483648.0 ?
        std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(scaled);
    return static_cast<std::uint8_t>(integer);
}

float actoranimation::timingCrossing(const actormodel::Resource* resource, std::string_view name, double previous, double current) {
    const float first = sampleTiming(resource, name, previous);
    if (first == -10000) return -1;
    const auto& bytes = resource->bytes;
    const auto count = half(bytes, 0), records = word(bytes, 4);
    name = name.substr(0, name.find('\0'));
    for (unsigned i = 0; i < count; ++i) {
        const auto offset = records + i * 36;
        std::string_view label(reinterpret_cast<const char*>(bytes.data() + offset), 30);
        label = label.substr(0, label.find('\0'));
        if (label.find(name) == std::string_view::npos) continue;
        const unsigned keys = half(bytes, offset + 30), start = half(bytes, offset + 32), stride = half(bytes, offset + 34);
        const auto at = [&](unsigned index) {
            const auto bits = word(bytes, 8 + index * 4);
            float value;
            std::memcpy(&value, &bits, 4);
            return value;
        };
        float maximum = -10000, beforeMaximum = -10000;
        if (keys > 1) {
            unsigned begin = 0, end = 0;
            while (begin < keys && at(start + begin * stride) < previous) ++begin;
            while (end < keys && at(start + end * stride) < current) ++end;
            for (unsigned key = begin; key < end; ++key) {
                const auto value = at(start + key * stride + 1);
                if (maximum < value) {
                    maximum = value;
                    beforeMaximum = key ? at(start + (key - 1) * stride + 1) : -10000;
                }
            }
        }
        const float last = sampleTiming(resource, name, current);
        if (last == -10000) return 0;
        const auto quantize = [](float value) {
            const double scaled = 100.0 * double(value);
            const auto integer = !std::isfinite(scaled) || scaled < -2147483648.0 || scaled >= 2147483648.0 ?
                std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(scaled);
            return 0.01f * static_cast<float>(integer);
        };
        if (quantize(first) <= maximum && quantize(last) < maximum) return std::max(maximum - beforeMaximum, 0.0f);
        return 0;
    }
    return 0;
}
