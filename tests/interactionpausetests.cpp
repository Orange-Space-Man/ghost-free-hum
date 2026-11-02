#include "game/effects/particleemitterstate.h"
#include "game/effects/modeleffectpool.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    struct Simulation final : particles::Simulation {
        unsigned callbacks = 0, emissions = 0, advances = 0;
        void callback(particles::Emitter&) override { ++callbacks; }
        void emit(particles::Emitter&) override { ++emissions; }
        void advance(particles::Emitter&) override { ++advances; }
    };

    particles::Emitter& allocate(particles::EmitterPool& pool) {
        auto* emitter = pool.emitters.free.take();
        require(emitter != nullptr, "The test emitter pool must have capacity.");
        emitter->reset();
        emitter->flags = 0xe;
        emitter->position = {1, 2, 3};
        emitter->fieldB8 = 17;
        pool.destination->append(*emitter);
        return *emitter;
    }

    void checkEmitters() {
        particles::EmitterPool pool(5, 0, 0);
        auto& first = allocate(pool);
        auto& second = allocate(pool);
        second.setRunning(false);
        pool.setPaused(true);
        require(first.flags == 0x10e && second.flags == 0x108,
            "Interaction pause must preserve visibility and each emitter's running state.");
        Simulation simulation;
        pool.update(simulation);
        require(simulation.emissions == 0 && simulation.advances == 0 && first.fieldB8 == 17 &&
            first.position == std::array<float, 3>{1, 2, 3} && pool.active[0].count == 2,
            "Paused emitters retain their simulation state and pool ownership.");
        auto& later = allocate(pool);
        require(later.flags == 0xe, "Original pause applies to current emitters, not future allocations.");
        pool.update(simulation);
        require(simulation.emissions == 1 && simulation.advances == 1,
            "A newly allocated emitter must still advance after an earlier pause operation.");
        pool.setPaused(true);
        require(later.flags == 0x10e, "Repeating the pause must visit newly allocated emitters.");
        pool.enterAlternate();
        auto& alternate = allocate(pool);
        pool.setPaused(false);
        require(first.flags == 0x10e && later.flags == 0x10e && alternate.flags == 0xe,
            "Pause traversal must use the original current destination ring only.");
        pool.setPaused(true);
        pool.update(simulation);
        require(simulation.emissions == 1 && simulation.advances == 1,
            "Alternate emitters must obey the same pause flag.");
        pool.leaveAlternate();
        pool.setPaused(false);
        require(first.flags == 0xe && second.flags == 8 && later.flags == 0xe && pool.active[2].count == 1,
            "Resume restores prior running bits and leaves retirement ownership intact.");
        pool.update(simulation);
        require(simulation.emissions == 3 && simulation.advances == 3 && pool.emitters.free.count == 2,
            "Resuming restarts running emitters and recycles retired alternate storage normally.");
    }

    void checkModels() {
        modeleffects::Definition definition;
        definition.capacity = 2;
        definition.lifetime = 9;
        modeleffects::Pool pool(definition);
        auto& active = *pool.owners[0];
        auto& inactive = *pool.owners[1];
        active.flags |= 1;
        active.elapsed = 3;
        pool.setPaused(true);
        require(active.flags == 0x17 && inactive.flags == 6,
            "Model pause must visit active owners only and preserve all other flags.");
        pool.advance(active, true);
        require(active.elapsed == 3, "Paused model effects must retain their lifetime clock.");
        active.setRunning(false);
        pool.setPaused(false);
        pool.advance(active, true);
        require(active.flags == 5 && active.elapsed == 3,
            "Resume must not restart a model that had its running bit cleared.");
        active.setRunning(true);
        pool.advance(active, true);
        require(active.elapsed == 4, "Model lifetime must continue from its retained frame after resume.");
        pool.setPaused(true);
        inactive.flags |= 1;
        require(inactive.flags == 7, "New model activations must not inherit a previous global pause.");
        pool.setPaused(true);
        require(inactive.flags == 0x17, "Repeated model pause must include newly active owners.");
        active.release();
        pool.setPaused(false);
        require(active.flags == 14 && inactive.flags == 7,
            "Release during pause must retain original reset flags and skip inactive owners on resume.");
    }
}

int main() {
    try {
        checkEmitters();
        checkModels();
        std::cout << "Original interaction effect pause/resume verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
