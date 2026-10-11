#include "game/effects/particlesimulation.h"

particles::NativeSimulation::NativeSimulation(const std::vector<std::uint8_t>& executable, std::uint32_t& randomState)
    : emission(executable), forces(executable), randomState(randomState) {}

void particles::NativeSimulation::callback(Emitter& emitter) {
    if (emitter.fieldC4) emitter.fieldC4->advance(emitter);
}

void particles::NativeSimulation::emit(Emitter& emitter) { emission.emit(emitter, randomState); }

void particles::NativeSimulation::advance(Emitter& emitter) { particles::advance(emitter, forces, randomState); }
