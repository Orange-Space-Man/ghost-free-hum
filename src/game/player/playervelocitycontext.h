#pragma once

#include "game/player/playervelocityupdate.h"
#include "game/player/playeractioncontext.h"

namespace playermotion {
    class VelocityContext {
    public:
        explicit VelocityContext(const std::vector<std::uint8_t>& executable);
        VelocityInput build(const playeractor::Player& player, const mapcollision::State& collision) const;

    private:
        std::array<float, 29> constants;
        playeractions::Context actions;
    };
}
