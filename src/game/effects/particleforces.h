#pragma once

#include "game/effects/particleemitterstate.h"

namespace particles {
    class Forces {
    public:
        explicit Forces(const std::vector<std::uint8_t>& executable);
        void apply(const Parameters& parameters, Node& node, std::uint32_t& randomState) const;

    private:
        std::array<std::array<std::uint16_t, 1024>, 6> noise;
    };
}
