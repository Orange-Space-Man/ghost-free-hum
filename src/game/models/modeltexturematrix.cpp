#include "game/models/modeltexturematrix.h"

#include <cstring>
#include <stdexcept>

bool modelmaterial::readTransform(const std::vector<std::uint8_t>& bytes, int index, TextureTransform& transform, std::string& error) {
    error.clear();
    try {
        const auto word = [&](std::size_t offset) {
            if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated generated texture matrix.");
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i) value = (value << 8) | bytes[offset + i];
            return value;
        };
        if (index < 0) throw std::runtime_error("Negative generated texture matrix index.");
        const auto offset = std::size_t(word(0x44)) + std::size_t(index) * 0x30;
        TextureTransform result;
        result.flags = word(offset);
        if (result.flags & 2) for (unsigned i = 0; i < result.values.size(); ++i) {
            const auto bits = word(offset + 4 + i * 4);
            std::memcpy(&result.values[i], &bits, sizeof(bits));
        }
        transform = result;
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

dooranimation::Matrix modelmaterial::evaluateTransform(const TextureTransform& transform,
    const dooranimation::Matrix& basis, const originalmath::FloatTrig& trig) {
    if (!(transform.flags & 2)) return basis;
    const auto& v = transform.values;
    const auto angles = trig.sinCos(v[2]);
    const float sine = angles[0], cosine = angles[1];
    const float sxSin = v[0] * sine, negativeSxCos = -v[0] * cosine;
    const float centerYSine = v[6] * sxSin, centerXCosine = v[5] * negativeSxCos;
    const float xSum = centerXCosine + centerYSine;
    const float xCenter = v[5] + xSum;
    const float syCos = v[1] * cosine, negativeSySin = -v[1] * sine;
    const float centerYCosine = v[6] * syCos, centerXSine = v[5] * negativeSySin;
    const float yDifference = centerXSine - centerYCosine;
    const float yCenter = v[6] + yDifference;
    const dooranimation::Matrix local{v[0] * cosine, -v[0] * sine, 0, v[3] + xCenter,
        v[1] * sine, syCos, 0, v[4] + yCenter, 0, 0, 1, 0};
    return dooranimation::concatenate(local, basis);
}
