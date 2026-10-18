#pragma once

#include "game/effects/particleeffectfactory.h"

namespace particles {
    class Batch {
    public:
        Batch(const std::vector<std::uint8_t>& executable, ArchiveResources& resources, EmitterPool& pool,
            unsigned count, unsigned type);
        void update();
        void emit(const std::array<float, 3>& position);
        const std::vector<Emitter*>& entries() const;
        unsigned cursor = 0;

    private:
        bool allocate();
        const std::vector<std::uint8_t>& executable;
        ArchiveResources& resources;
        EmitterPool& pool;
        unsigned type;
        bool initialized;
        std::vector<Emitter*> slots;
    };
}
