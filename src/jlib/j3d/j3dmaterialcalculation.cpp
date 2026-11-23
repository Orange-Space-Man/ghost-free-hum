#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dkeyanimation.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <stdexcept>

void j3dmodel::animateMaterialColors(Material& material) {
    if (!material.animation) return;
    for (unsigned slot = 0; slot < material.animation->colors.size(); ++slot) {
        const auto& binding = material.animation->colors[slot];
        if (!binding || !binding->animation) continue;
        const auto& animation = *binding->animation;
        if (animation.kind != AnimationKind::Color || !material.color)
            throw std::runtime_error("J3D color animation requires its original material color block.");
        const auto& group = animation.groups[0];
        if (binding->index >= group.count) throw std::runtime_error("J3D color animation exceeds its key table.");
        for (unsigned channel = 0; channel < 4; ++channel) {
            const auto descriptor = group.table + binding->index * 24 + channel * 6;
            const auto& bytes = *animation.bytes;
            const unsigned count = (unsigned(bytes.at(descriptor)) << 8) | bytes.at(descriptor + 1);
            const float value = sampleAnimationKey(animation, group, descriptor, channel, true, 0);
            auto& output = material.color->material[slot][channel];
            if (count <= 1) output = static_cast<std::uint8_t>(static_cast<int>(value));
            else if (value < 0) output = 0;
            else if (value > 255) output = 255;
            else if (value >= 0 && value <= 255) output = static_cast<std::uint8_t>(value);
        }
    }
}

void j3dmodel::animateTevColors(Material& material) {
    if (!material.animation || !material.tev) return;
    const auto animate = [&](const auto& bindings, auto& colors, unsigned section, int minimum, int maximum) {
        for (unsigned slot = 0; slot < bindings.size(); ++slot) {
            if (!bindings[slot]) continue;
            const auto& binding = *bindings[slot];
            const auto& animation = *binding.animation;
            const auto& group = animation.groups[section];
            for (unsigned component = 0; component < 4; ++component) {
                const auto value = sampleAnimationKey(animation, group,
                    group.table + binding.index * 28 + component * 6, component, true, 0);
                using Color = typename std::decay_t<decltype(colors[slot])>::value_type;
                colors[slot][component] = static_cast<Color>(std::clamp<int>(int(value), minimum, maximum));
            }
        }
    };
    animate(material.animation->tevColors, material.tev->colors, 0, -1024, 1023);
    animate(material.animation->tevConstants, material.tev->constants, 1, 0, 255);
}

namespace {
    float number(const std::array<std::uint8_t, 100>& bytes, unsigned offset) {
        const std::uint32_t bits = (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
            (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

void j3dmodel::calculateTexture(TextureMatrix& texture, const TransformTables& tables) {
    const auto& info = texture.info;
    Joint rotation;
    rotation.rotation[2] = static_cast<std::int16_t>((unsigned(info[24]) << 8) | info[25]);
    const auto basis = tables.transform(rotation);
    const float cosine = basis[0], sine = basis[4];
    const float scaleS = number(info, 16), scaleT = number(info, 20);
    const float centerS = number(info, 4), centerT = number(info, 8);
    Matrix transform{scaleS * cosine, -scaleS * sine, 0,
        ((number(info, 28) + centerS) + (centerS * -scaleS) * cosine) + (centerT * scaleS) * sine,
        scaleT * sine, scaleT * cosine, 0,
        (number(info, 32) + centerT) + ((centerS * -scaleT) * sine - (centerT * scaleT) * cosine),
        0, 0, 1, 0};
    const auto mode = info[1];
    if (mode >= 2 && mode <= 5) {
        Matrix projected;
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 4; ++column) {
                projected[row * 4 + column] = ((transform[row * 4 + 3] * number(info, 36 + (12 + column) * 4) +
                    transform[row * 4 + 2] * number(info, 36 + (8 + column) * 4)) +
                    transform[row * 4] * number(info, 36 + column * 4)) +
                    transform[row * 4 + 1] * number(info, 36 + (4 + column) * 4);
            }
        }
        transform = projected;
    }
    texture.matrix = mode == 1 || mode == 2 || mode == 3 || mode == 5 ?
        concatenateMatrix(transform, texture.effectMatrix) : transform;
}

void j3dmodel::calculateMaterial(Material& material, const Matrix& joint, const Matrix& camera,
    std::uint32_t systemFlags, const TransformTables& tables) {
    if (!material.texture) throw std::runtime_error("J3D material calculation requires its texture block.");
    constexpr Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    const bool independent = (systemFlags & 12) == 12;
    for (auto& entry : material.texture->matrices) {
        if (!entry) continue;
        auto& texture = *entry;
        switch (texture.info[1]) {
        case 1:
            texture.effectMatrix = independent ? camera : concatenateMatrix(camera, joint);
            texture.effectMatrix[3] = texture.effectMatrix[7] = texture.effectMatrix[11] = 0;
            break;
        case 2:
            texture.effectMatrix = independent ? identity : joint;
            break;
        case 3:
            texture.effectMatrix = independent ? camera : concatenateMatrix(camera, joint);
            break;
        case 5:
            if (material.texture->full && !independent) {
                texture.effectMatrix = joint;
                texture.effectMatrix[3] = texture.effectMatrix[7] = texture.effectMatrix[11] = 0;
            }
            break;
        }
        calculateTexture(texture, tables);
    }
}
