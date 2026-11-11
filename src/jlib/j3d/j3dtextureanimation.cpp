#include "jlib/j3d/j3dtextureanimation.h"
#include "jlib/j3d/j3dkeyanimation.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void store(std::array<std::uint8_t, 100>& bytes, unsigned offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
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
            store(info, 16, sampleAnimationKey(animation, group, table, 0, false, 1));
            store(info, 20, sampleAnimationKey(animation, group, table + 18, 0, false, 1));
            const auto rotation = static_cast<std::uint32_t>(static_cast<std::int32_t>(sampleAnimationKey(animation, group, table + 42, 1, true, 0)));
            const auto angle = animation.angleScale < 32 ? rotation << animation.angleScale : 0;
            info[24] = static_cast<std::uint8_t>(angle >> 8); info[25] = static_cast<std::uint8_t>(angle);
            store(info, 28, sampleAnimationKey(animation, group, table + 12, 2, false, 0));
            store(info, 32, sampleAnimationKey(animation, group, table + 30, 2, false, 0));
        }
    };
    apply(material.animation->matrices, material.texture->matrices, 0);
    apply(material.animation->postMatrices, material.texture->postMatrices, 1);
}
