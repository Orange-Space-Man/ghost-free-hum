#pragma once

#include "game/effects/modeleffectpool.h"
#include "game/effects/elementparticlepool.h"
#include "game/world/mapcollisionstate.h"
#include "game/events/runtimeflags.h"

#include <functional>

namespace modeleffects {
    class RiverOwner final : public Owner {
    public:
        RiverOwner();
        void reset() override;
        void setFrame(double frame) override;
        std::int32_t secondFrame = 0, hits = 0, transition = 0;
        std::array<float, 3> position{};
        particles::Emitter* emitter = nullptr;
    };

    class RiverPool final : public Pool {
    public:
        using Sound = std::function<void(unsigned, const std::array<float, 3>&, unsigned)>;
        RiverPool(particles::ArchiveResources& resources, particles::EmitterPool& emitters,
            const elements::Pool& elements, const elements::Projectile& ice, const elements::Projectile& water,
            gameflags::State& flags, mapcollision::State& collision, Sound sound);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available(const j3dmodel::Matrix& matrix) override;
        void advance(Owner& owner, bool visible) override;
        std::array<std::shared_ptr<j3dmodel::Animation>, 2> textures;

    private:
        particles::ArchiveResources& resources;
        particles::EmitterPool& emitters;
        const elements::Pool& elements;
        const elements::Projectile& ice;
        const elements::Projectile& water;
        gameflags::State& flags;
        mapcollision::State& collision;
        Sound sound;
        void createIce(RiverOwner& owner);
        void updateInteraction(RiverOwner& owner);
    };
}
