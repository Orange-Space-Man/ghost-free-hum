#pragma once

#include "game/actors/actoreffectstate.h"
#include "game/player/playerweaponparameters.h"
#include "game/player/playerpipemodel.h"

namespace playerweapons {
    class Owner {
    public:
        Owner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
            playermodels::PipeParameters& parameters);
        ~Owner();
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        std::int32_t actorType = -1, firstType = -1, secondType = -1;
        std::uint32_t fieldC;
        std::unique_ptr<Part> first, second;
        std::unique_ptr<playermodels::PipeModel> pipe;
        std::uint8_t field1C = 0;
        actoreffects::State effect;
    };
}
