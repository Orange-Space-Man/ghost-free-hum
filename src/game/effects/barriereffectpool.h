#pragma once

#include "game/effects/modeleffectpool.h"

namespace modeleffects {
    class BarrierOwner final : public Owner {
    public:
        BarrierOwner();
        void reset() override;
        void setFrame(double frame) override;
        float animationFrame = 0, speed = 1;
        particles::Resource particle, texture;
    };

    class BarrierPool final : public Pool {
    public:
        BarrierPool(unsigned variant, particles::EmitterPool& emitters);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> textureAnimation;

    private:
        unsigned variant;
        particles::EmitterPool& emitters;
    };
}
