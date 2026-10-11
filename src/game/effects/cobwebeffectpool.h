#pragma once

#include "game/effects/modeleffectpool.h"
#include "game/player/playeractorview.h"

namespace modeleffects {
    class CobwebOwner final : public Owner {
    public:
        CobwebOwner();
        void reset() override;
        void setFrame(double frame) override;
        std::unique_ptr<j3dmodel::ModelInstance> secondary;
        std::shared_ptr<j3dmodel::Animation> color;
        float inverseDistance = 1;
        std::int32_t step = 1;
        std::array<float, 3> anchor{};
    };

    class CobwebPool final : public Pool {
    public:
        CobwebPool(const playeractor::View& player, particles::EffectFactory& particles);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        void update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility) override;
        std::shared_ptr<j3dmodel::Animation> secondaryTransform;

    private:
        const playeractor::View& player;
        particles::EffectFactory& particles;
    };

    CobwebOwner* createCobweb(Factory& factory, const j3dmodel::Matrix& matrix);
}
