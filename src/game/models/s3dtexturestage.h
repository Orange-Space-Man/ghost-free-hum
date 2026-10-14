#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace s3dtexturestage {
    struct Stage {
        unsigned kind = 0, sampler = 0;
        float scaleS = 1, scaleT = 1, rotation = 0;
        float translationS = 0, translationT = 0, pivotS = 0, pivotT = 0;
    };

    using Matrix = std::array<float, 8>;

    Stage read(const std::vector<std::uint8_t>& data, std::size_t offset);
    Matrix normalMatrix(const Stage& stage, const std::array<float, 12>& modelView,
        const std::vector<float>& trigonometry);
    std::array<float, 2> coordinate(const Matrix& matrix, const std::array<float, 3>& normal);
}
