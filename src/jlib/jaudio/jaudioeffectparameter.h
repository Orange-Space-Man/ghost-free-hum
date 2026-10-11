#pragma once

#include <cstdint>

namespace jaudio {
    struct EffectParameter {
        explicit EffectParameter(float value = 0) : target(value), current(value) {}
        void set(float value, std::uint32_t frames);
        void advance();
        float target = 0, current = 0, step = 0;
        std::uint32_t remaining = 0;
    };
}
