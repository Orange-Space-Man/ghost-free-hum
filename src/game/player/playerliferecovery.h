#pragma once

#include "game/player/playeractor.h"
#include "game/effects/particleeffectfactory.h"

#include <functional>

namespace playerevents {
    struct LifeEnvironment {
        std::int32_t& audioLife;
        particles::EffectFactory& effects;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::function<void(const std::array<float, 3>&)> deathSound;
        std::function<void(const std::array<float, 3>&, const std::array<float, 3>&,
            std::uint8_t, std::uint16_t, double, std::uint8_t)> scatterMoney;
    };

    void setLife(playeractor::Player& player, std::int16_t life, LifeEnvironment& environment);
    void recoverLife(playeractor::Player& player, std::uint32_t amount, LifeEnvironment& environment);
    unsigned loseMoney(playeractor::Player& player, std::uint32_t amount, LifeEnvironment& environment);
}
