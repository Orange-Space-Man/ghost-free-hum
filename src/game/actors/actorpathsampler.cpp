#include "game/actors/actorpathsampler.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (half(bytes, offset) << 16) | half(bytes, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const auto bits = word(bytes, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    float interpolate(double frame, float start, float end, float first, float second, float outgoing, float incoming) {
        const float startTime = start / 30.0f;
        const float elapsed = static_cast<float>(frame / 30.0) - startTime;
        const float inverse = 1.0f / (end / 30.0f - startTime);
        const float twiceSquare = (2.0f * elapsed) * elapsed;
        const float square = elapsed * elapsed;
        const float threeSquare = (3.0f * elapsed) * elapsed;
        const float positive = inverse * (inverse * (inverse * (elapsed * twiceSquare)));
        const float negative = inverse * (inverse * (((-2.0f * elapsed) * elapsed) * elapsed));
        const float curvature = inverse * (inverse * threeSquare);
        const float tangent = inverse * (inverse * (elapsed * square));
        const float firstValue = (1.0f + (positive - curvature)) * first;
        const float secondValue = (inverse * negative + curvature) * second;
        const float outgoingValue = (elapsed + (tangent - inverse * twiceSquare)) * outgoing;
        const float incomingValue = (tangent - inverse * square) * incoming;
        return incomingValue + (outgoingValue + (firstValue + secondValue));
    }

    template<class Read>
    float sample(const std::vector<std::uint8_t>& bytes, unsigned descriptor, double frame, const Read& at) {
        const auto count = half(bytes, descriptor), first = half(bytes, descriptor + 2), stride = half(bytes, descriptor + 4);
        if (stride == 1) return at(first);
        unsigned index = 0;
        while (index < count && !(at(first + index * stride) >= frame)) ++index;
        if (!index) return at(first + 1);
        if (index == count) return at(first + (index - 1) * stride + 1);
        const auto left = first + (index - 1) * stride, right = first + index * stride;
        return interpolate(frame, at(left), at(right), at(left + 1), at(right + 1),
            at(left + (stride == 4 ? 3 : 2)), at(right + 2));
    }
}

void actoranimation::samplePath(const actormodel::Resource& path, double frame, std::array<float, 3>& position,
    std::array<std::int16_t, 3>* rotation) {
    const auto& bytes = path.bytes;
    const auto floats = word(bytes, 0x28);
    const auto floatAt = [&](unsigned index) { return number(bytes, std::size_t(floats) + std::size_t(index) * 4); };
    for (unsigned axis = 0; axis < 3; ++axis) position[axis] = sample(bytes, 4 + axis * 6, frame, floatAt);
    if (!rotation) return;
    const auto rotations = word(bytes, 0x2c);
    const auto rotationAt = [&](unsigned index) {
        return static_cast<float>(static_cast<std::int16_t>(half(bytes, std::size_t(rotations) + std::size_t(index) * 2)));
    };
    for (unsigned axis = 0; axis < 3; ++axis) {
        const auto value = sample(bytes, 0x16 + axis * 6, frame, rotationAt);
        const auto integer = !std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0 ?
            std::int32_t(-2147483647 - 1) : static_cast<std::int32_t>(value);
        (*rotation)[axis] = static_cast<std::int16_t>(static_cast<std::uint32_t>(integer) << 4);
    }
}

float actoranimation::sampleFloatTrack(const std::vector<std::uint8_t>& bytes, unsigned descriptor, unsigned values, double frame) {
    const auto count = half(bytes, descriptor), first = half(bytes, descriptor + 2), stride = half(bytes, descriptor + 4);
    const auto at = [&](unsigned index) { return number(bytes, std::size_t(values) + std::size_t(index) * 4); };
    if (stride == 1) return at(first);
    unsigned index = 0;
    while (index < count && at(first + index * stride) < frame) ++index;
    if (!index) return at(first + 1);
    if (index == count) return at(first + (index - 1) * stride + 1);
    const auto left = first + (index - 1) * stride, right = first + index * stride;
    return interpolate(frame, at(left), at(right), at(left + 1), at(right + 1),
        at(left + (stride == 4 ? 3 : 2)), at(right + 2));
}

bool actoranimation::pathDisplacement(const actormodel::Resources& resources, unsigned model, unsigned key,
    double previous, double current, bool reverse, std::array<float, 3>& displacement) {
    model = static_cast<std::uint16_t>(model);
    key = static_cast<std::uint16_t>(key);
    const auto path = resources.path(model, key);
    if (!path) return false;
    const double first = reverse ? current : previous, second = reverse ? previous : current;
    std::array<float, 3> start, end;
    samplePath(*path, first, start);
    samplePath(*path, second, end);
    if (reverse || first <= second) {
        for (unsigned axis = 0; axis < 3; ++axis) displacement[axis] = end[axis] - start[axis];
    }
    if ((!reverse && first <= second) || (reverse && second <= first)) return true;
    const auto& entry = resources.entry(model);
    const auto& animation = entry.archive->keys.at(key);
    if (!animation) throw std::runtime_error("Original path wrap requires its key resource.");
    std::array<float, 3> terminal;
    samplePath(*path, half(animation->bytes, 4), terminal);
    for (unsigned axis = 0; axis < 3; ++axis) {
        displacement[axis] = reverse ? displacement[axis] - terminal[axis] : displacement[axis] + terminal[axis];
    }
    return true;
}
