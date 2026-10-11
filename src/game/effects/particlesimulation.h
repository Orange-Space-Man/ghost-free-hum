#pragma once

#include "game/effects/particleemission.h"
#include "game/effects/particleadvancement.h"

namespace particles {
    class NativeSimulation final : public Simulation {
    public:
        NativeSimulation(const std::vector<std::uint8_t>& executable, std::uint32_t& randomState);
        void callback(Emitter& emitter) override;
        void emit(Emitter& emitter) override;
        void advance(Emitter& emitter) override;

    private:
        Emission emission;
        Forces forces;
        std::uint32_t& randomState;
    };
}
