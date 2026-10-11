#pragma once

#include "game/effects/particleemitterstate.h"

#include <string_view>

namespace particles {
    struct SuctionCallback;
    class ArchiveResources {
    public:
        virtual ~ArchiveResources() = default;
        virtual Resource get(std::uint32_t type, std::string_view name) = 0;
    };

    class EffectFactory {
    public:
        EffectFactory(const std::vector<std::uint8_t>& executable, ArchiveResources& resources, EmitterPool& pool);
        Emitter* create(unsigned type, const std::array<float, 3>& position, const std::array<float, 3>& vector);
        Emitter* createDamage(unsigned type, const std::array<float, 3>& position, SuctionCallback& callback);
        Emitter* createLifeLoss(unsigned type, const std::array<float, 3>& position, std::int32_t amount);
        void createExplosion(unsigned type, const std::array<float, 3>& position);
        std::array<std::uint32_t, 50> history{};

    private:
        const std::vector<std::uint8_t>& executable;
        ArchiveResources& resources;
        EmitterPool& pool;
    };
}
