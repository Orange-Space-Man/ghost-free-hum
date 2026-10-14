#include "game/models/s3dfog.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float coefficient(float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        bits &= 0xfffff000;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

s3dfog::State s3dfog::prepare(const s3denvironment::Fog& fog, const std::array<float, 16>& projection,
    float nearPlane, float farPlane) {
    State state;
    state.type = fog.type & 7;
    state.color = fog.color;
    if (state.type == 0) return state;
    float b = 0.5f, a = 0, c = 0;
    if (farPlane != nearPlane && fog.end != fog.start) {
        b = farPlane / (farPlane - nearPlane);
        a = (farPlane * nearPlane) / ((farPlane - nearPlane) * (fog.end - fog.start));
        c = fog.start / (fog.end - fog.start);
    }
    if (!std::isfinite(b) || !std::isfinite(a) || !std::isfinite(c) || b <= 0)
        throw std::runtime_error("Invalid original fog projection.");
    int exponent = 0;
    while (b > 1) { b *= 0.5f; ++exponent; }
    while (b < 0.5f) { b *= 2; --exponent; }
    if (exponent < -1 || exponent > 30) throw std::runtime_error("Unsupported original fog shift.");
    state.shift = unsigned(exponent + 1);
    state.a = coefficient(a / float(1U << state.shift));
    state.c = coefficient(c);
    state.magnitude = static_cast<unsigned>(8388638.0f * b) & 0xffffff;
    state.rangeEnabled = fog.rangeEnabled;
    state.center = (fog.rangeCenter + 340) & 1023;
    if (state.rangeEnabled) {
        if (!fog.rangeWidth || projection[0] == 0) throw std::runtime_error("Invalid fog range projection.");
        float depth, extent;
        if (projection[15] == 0) {
            depth = projection[11] / (projection[10] - 1);
            extent = depth * (1 + projection[2]) / projection[0];
        } else {
            depth = (1 + projection[11]) / projection[10];
            extent = -(projection[3] - 1) / projection[0];
        }
        if (depth == 0 || !std::isfinite(depth) || !std::isfinite(extent))
            throw std::runtime_error("Invalid fog range scale.");
        for (unsigned index = 0; index < state.range.size(); ++index) {
            const float x = float((index + 1) * 32) * (2.0f / float(fog.rangeWidth & 0xffff)) * extent;
            const float correction = std::sqrt(1 + (x * x) / (depth * depth));
            state.range[index] = static_cast<unsigned>(256.0f * correction) & 4095;
        }
    }
    return state;
}
