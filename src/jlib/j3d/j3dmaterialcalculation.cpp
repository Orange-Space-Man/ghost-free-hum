#include "jlib/j3d/j3dmaterialcalculation.h"

#include <cstring>
#include <stdexcept>

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
