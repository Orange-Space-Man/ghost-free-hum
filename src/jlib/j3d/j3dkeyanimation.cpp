#include "jlib/j3d/j3dkeyanimation.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    float number(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        const std::uint32_t bits = (half(bytes, offset) << 16) | half(bytes, offset + 2);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }
}

float j3dmodel::sampleAnimationKey(const j3dmodel::Animation& animation, const j3dmodel::AnimationGroup& group,
    unsigned descriptor, unsigned kind, bool shortValues, float fallback) {
    const auto& bytes = *animation.bytes;
    const unsigned count = half(bytes, descriptor), first = half(bytes, descriptor + 2);
    if (!count) return fallback;
    const auto value = [&](unsigned index) {
        if (first + index >= group.valueCounts[kind]) throw std::runtime_error("J3D animation key exceeds its value table.");
        const auto offset = group.values[kind] + (first + index) * (shortValues ? 2 : 4);
        return shortValues ? float(static_cast<std::int16_t>(half(bytes, offset))) : number(bytes, offset);
    };
    if (count == 1) return value(0);
    const unsigned stride = half(bytes, descriptor + 4) == 0 ? 3 : 4;
    const float frame = animation.frame;
    if (frame < value(0)) return value(1);
    if (frame >= value((count - 1) * stride)) return value((count - 1) * stride + 1);
    unsigned key = 0, remaining = count;
    while (remaining > 1) {
        const unsigned middle = remaining / 2;
        if (value((key + middle) * stride) <= frame) { key += middle; remaining -= middle; }
        else remaining = middle;
    }
    const unsigned a = key * stride, b = a + stride;
    const float time = frame - value(a), inverse = 1 / (value(b) - value(a));
    const float squared = inverse * time * time, cubed = time * squared * inverse;
    const float curved = 3 * squared * inverse;
    return value(b + 2) * (cubed - squared) + value(a + stride - 1) * (time + (cubed - 2 * squared)) +
        value(a + 1) * (1 + (2 * cubed * inverse - curved)) + value(b + 1) * (-2 * cubed * inverse + curved);
}
