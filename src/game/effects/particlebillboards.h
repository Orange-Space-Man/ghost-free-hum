#pragma once

#include "game/effects/particleemitterstate.h"
#include "game/effects/particleglowpool.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"

namespace particles {
    struct Billboard {
        std::array<std::array<float, 3>, 4> positions{};
        std::array<std::uint8_t, 4> color{}, secondary{};
        Resource texture;
        std::array<std::array<float, 2>, 4> coordinates{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
        unsigned mode = 0, blend = 0, depth = 0, cull = 2;
        float textureScale = 1;
        std::array<unsigned, 2> textureWrap{};
    };

    class Billboards {
    public:
        explicit Billboards(const std::vector<std::uint8_t>& executable);
        std::vector<Billboard> prepare(EmitterPool& pool, const roomlocation::Rooms& rooms,
            const roomtraversal::State& visibility, const std::array<float, 12>& view) const;

    private:
        std::array<float, 4096> sine{}, cosine{};
        void append(Emitter& emitter, const std::array<float, 12>& view, std::vector<Billboard>& result) const;
    };

    std::vector<Billboard> prepareGlows(GlowPool& pool, Resource texture, std::uint32_t& randomState,
        const std::array<float, 12>& view);
}
