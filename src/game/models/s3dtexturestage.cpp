#include "game/models/s3dtexturestage.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const std::uint32_t bits = (std::uint32_t(data[offset]) << 24) | (std::uint32_t(data[offset + 1]) << 16) |
            (std::uint32_t(data[offset + 2]) << 8) | data[offset + 3];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) throw std::runtime_error("Nonfinite S3D texture transform.");
        return value;
    }
}

s3dtexturestage::Stage s3dtexturestage::read(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 32) throw std::runtime_error("Truncated S3D texture stage.");
    Stage stage;
    stage.kind = data[offset];
    stage.sampler = (unsigned(data[offset + 2]) << 8) | data[offset + 3];
    if (stage.kind > 2) throw std::runtime_error("Unsupported S3D texture coordinate generator.");
    if (stage.kind == 2) {
        stage.scaleS = number(data, offset + 4);
        stage.scaleT = number(data, offset + 8);
        stage.rotation = number(data, offset + 12);
        stage.translationS = number(data, offset + 16);
        stage.translationT = number(data, offset + 20);
        stage.pivotS = number(data, offset + 24);
        stage.pivotT = number(data, offset + 28);
    }
    return stage;
}

s3dtexturestage::Matrix s3dtexturestage::normalMatrix(const Stage& stage, const std::array<float, 12>& modelView,
    const std::vector<float>& trigonometry) {
    if (stage.kind != 2 || trigonometry.size() < 5120) throw std::runtime_error("Missing S3D normal texture dependencies.");
    const float angle = (65536.0f * stage.rotation) / 360.0f;
    if (!std::isfinite(angle) || angle < -2147483648.0 || angle >= 4294967296.0)
        throw std::runtime_error("S3D texture rotation is outside the original conversion range.");
    const auto integral = angle < 0 ? std::uint32_t(static_cast<std::int32_t>(angle)) : static_cast<std::uint32_t>(angle);
    const unsigned index = (integral >> 4) & 4095;
    const float sine = trigonometry[index], cosine = trigonometry[index + 1024];
    const float ss = stage.scaleS * cosine, st = -stage.scaleS * sine;
    const float ts = stage.scaleT * sine, tt = stage.scaleT * cosine;
    const float offsetS = stage.translationS + (stage.pivotS + (-stage.pivotS * ss - stage.pivotT * st));
    const float offsetT = stage.translationT + (stage.pivotT + (-stage.pivotS * ts - stage.pivotT * tt));
    Matrix result;
    for (unsigned axis = 0; axis < 3; ++axis) {
        result[axis] = ss * modelView[axis] + st * modelView[4 + axis];
        result[4 + axis] = ts * modelView[axis] + tt * modelView[4 + axis];
    }
    result[3] = offsetS;
    result[7] = offsetT;
    return result;
}

std::array<float, 2> s3dtexturestage::coordinate(const Matrix& matrix, const std::array<float, 3>& normal) {
    return {matrix[0] * normal[0] + matrix[1] * normal[1] + matrix[2] * normal[2] + matrix[3],
        matrix[4] * normal[0] + matrix[5] * normal[1] + matrix[6] * normal[2] + matrix[7]};
}
