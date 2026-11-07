#include "jlib/j3d/j3dtextureanimation.h"

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

    void store(std::array<std::uint8_t, 100>& bytes, unsigned offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
    }

    float sample(const j3dmodel::Animation& animation, const j3dmodel::AnimationGroup& group,
        unsigned descriptor, unsigned kind, float fallback) {
        const auto& bytes = *animation.bytes;
        const unsigned count = half(bytes, descriptor), first = half(bytes, descriptor + 2);
        if (!count) return fallback;
        const auto value = [&](unsigned index) {
            if (first + index >= group.valueCounts[kind]) throw std::runtime_error("J3D texture key exceeds its value table.");
            const auto offset = group.values[kind] + (first + index) * (kind == 1 ? 2 : 4);
            return kind == 1 ? float(static_cast<std::int16_t>(half(bytes, offset))) : number(bytes, offset);
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
}

void j3dmodel::animateTextureMatrices(Material& material) {
    if (!material.animation) return;
    const auto apply = [&](auto& bindings, auto& matrices, unsigned section) {
        for (unsigned slot = 0; slot < bindings.size(); ++slot) {
            const auto& binding = bindings[slot];
            if (!binding || !binding->animation) continue;
            const auto& animation = *binding->animation;
            if (animation.kind != AnimationKind::Texture || !std::isfinite(animation.frame) || !matrices[slot])
                throw std::runtime_error("J3D texture animation requires its bound matrix and finite clock.");
            const auto& group = animation.groups[section];
            const auto table = group.table + binding->index * 54;
            auto& info = matrices[slot]->info;
            store(info, 16, sample(animation, group, table, 0, 1));
            store(info, 20, sample(animation, group, table + 18, 0, 1));
            const auto rotation = static_cast<std::uint32_t>(static_cast<std::int32_t>(sample(animation, group, table + 42, 1, 0)));
            const auto angle = animation.angleScale < 32 ? rotation << animation.angleScale : 0;
            info[24] = static_cast<std::uint8_t>(angle >> 8); info[25] = static_cast<std::uint8_t>(angle);
            store(info, 28, sample(animation, group, table + 12, 2, 0));
            store(info, 32, sample(animation, group, table + 30, 2, 0));
        }
    };
    apply(material.animation->matrices, material.texture->matrices, 0);
    apply(material.animation->postMatrices, material.texture->postMatrices, 1);
}
