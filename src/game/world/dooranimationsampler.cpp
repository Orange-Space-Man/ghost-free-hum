#include "game/world/dooranimationsampler.h"

#include <cstring>
#include <stdexcept>

namespace {
    float lastFrame(const dooranimation::Resource& resource) {
        if (!resource.loaded()) throw std::runtime_error("Door animation resource is not loaded.");
        const std::uint32_t bits = resource.headerValue() - 1U;
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return static_cast<float>(value);
    }

    int half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated door animation channel.");
        const int value = (int(bytes[offset]) << 8) | bytes[offset + 1];
        return value < 32768 ? value : value - 65536;
    }

    float scalar(const std::vector<std::uint8_t>& bytes, std::size_t base, int index) {
        const auto address = static_cast<std::int64_t>(base) + static_cast<std::int64_t>(index) * 4;
        if (address < 0) throw std::runtime_error("Door animation key precedes its resource.");
        const auto offset = static_cast<std::size_t>(address);
        if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated door animation key.");
        const std::uint32_t bits = (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
            (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

double dooranimation::clampFrame(const Resource& resource, double frame) {
    const float end = lastFrame(resource);
    return end < frame ? static_cast<double>(end) : frame;
}

void dooranimation::updateFrame(const Resource& resource, float& frame) {
    const float end = lastFrame(resource);
    if (end < frame) frame = resource.headerByte(1) == 0 ? end : 0.0f;
}

float dooranimation::interpolate(float frame, const Key& first, const Key& second) {
    constexpr float timeScale = 0x1.111112p-6f;
    const float start = first.frame * timeScale;
    const float current = frame * timeScale;
    const float end = second.frame * timeScale;
    const float elapsed = current - start;
    const float duration = end - start;
    const float inverse = 1.0f / duration;
    float twiceSquare = 2.0f * elapsed;
    twiceSquare = twiceSquare * elapsed;
    const float square = elapsed * elapsed;
    float threeSquare = 3.0f * elapsed;
    threeSquare = threeSquare * elapsed;
    float firstWeight = elapsed * twiceSquare;
    firstWeight = inverse * firstWeight;
    firstWeight = inverse * firstWeight;
    firstWeight = inverse * firstWeight;
    float secondWeight = -2.0f * elapsed;
    secondWeight = secondWeight * elapsed;
    secondWeight = elapsed * secondWeight;
    secondWeight = inverse * secondWeight;
    secondWeight = inverse * secondWeight;
    secondWeight = inverse * secondWeight;
    threeSquare = inverse * threeSquare;
    threeSquare = inverse * threeSquare;
    float tangentCube = elapsed * square;
    tangentCube = inverse * tangentCube;
    tangentCube = inverse * tangentCube;
    const float tangentTwiceSquare = inverse * twiceSquare;
    firstWeight = firstWeight - threeSquare;
    firstWeight = 1.0f + firstWeight;
    secondWeight = secondWeight + threeSquare;
    const float tangentSquare = inverse * square;
    float outgoingWeight = tangentCube - tangentTwiceSquare;
    outgoingWeight = elapsed + outgoingWeight;
    const float incomingWeight = tangentCube - tangentSquare;
    const float firstValue = first.value * firstWeight;
    const float secondValue = second.value * secondWeight;
    const float outgoing = first.outgoing * outgoingWeight;
    const float incoming = second.incoming * incomingWeight;
    const float values = firstValue + secondValue;
    const float withOutgoing = outgoing + values;
    return incoming + withOutgoing;
}

float dooranimation::sampleChannel(const Resource& resource, unsigned track, unsigned channel, float frame) {
    if (!resource.loaded() || channel >= 9) throw std::runtime_error("Invalid door animation channel request.");
    const auto& bytes = resource.bytes();
    const auto descriptor = std::size_t(resource.sectionOffset(3)) + std::size_t(track) * 54 + channel * 6;
    const int count = half(bytes, descriptor);
    const int offset = half(bytes, descriptor + 2);
    const int stride = half(bytes, descriptor + 4) == 0 ? 3 : 4;
    const auto base = resource.sectionOffset(channel / 3);
    if (count == 1) return scalar(bytes, base, offset);
    for (int i = 0; i < count; ++i) {
        if (frame < scalar(bytes, base, offset + i * stride)) {
            const auto readKey = [&](int key) {
                const int index = offset + key * stride;
                return Key{scalar(bytes, base, index), scalar(bytes, base, index + 1),
                    scalar(bytes, base, index + 2), scalar(bytes, base, index + (stride == 3 ? 2 : 3))};
            };
            return interpolate(frame, readKey(i - 1), readKey(i));
        }
    }
    throw std::runtime_error("Door animation endpoint caller state is not ported yet.");
}
