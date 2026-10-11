#pragma once

#include "game/effects/particleemitterstate.h"

namespace particles {
    class Emission {
    public:
        explicit Emission(const std::vector<std::uint8_t>& executable);
        void emit(Emitter& emitter, std::uint32_t& randomState);
        std::array<unsigned, 2> angles{};
        float angleSine = 0;

    private:
        std::array<float, 4096> sine, cosine;
        std::array<float, 3> randomVector(std::uint32_t& randomState);
    };
}
