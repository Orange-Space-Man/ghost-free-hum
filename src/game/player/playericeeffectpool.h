#pragma once

#include "game/effects/modeleffectpool.h"

namespace modeleffects {
    class PlayerIceOwner final : public Owner {
    public:
        PlayerIceOwner();
        void reset() override;
        void setMatrix(const j3dmodel::Matrix& matrix) override;
        void setFrame(double frame) override;
        std::int32_t textureFrame = 0;
        bool resumeAtSix = true;
        std::array<particles::Emitter*, 2> emitters{};
    };

    class PlayerIcePool final : public Pool {
    public:
        PlayerIcePool(particles::ArchiveResources& resources, particles::EmitterPool& emitters);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        void update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility) override;
        std::shared_ptr<j3dmodel::Animation> textureAnimation;

    private:
        particles::ArchiveResources& resources;
        particles::EmitterPool& emitters;
    };
}
