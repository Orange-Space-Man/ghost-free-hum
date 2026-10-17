#pragma once

#include "jlib/jaudio/jaudioeffectcontrols.h"

#include <vector>

namespace jaudio {
    struct EffectParameters {
        EffectControls controls;
        std::array<std::uint16_t, 16> ports{};
        std::uint16_t pendingPorts = 0;
        EffectParameters* previous = nullptr;
        EffectParameters* next = nullptr;
    };

    class EffectParameterPool {
    public:
        explicit EffectParameterPool(unsigned count);
        EffectParameterPool(const EffectParameterPool&) = delete;
        EffectParameterPool& operator=(const EffectParameterPool&) = delete;
        EffectParameters* acquire();
        void release(EffectParameters& parameters);

    private:
        std::vector<EffectParameters> entries;
        EffectParameters* free = nullptr;
        EffectParameters* active = nullptr;
    };
}
