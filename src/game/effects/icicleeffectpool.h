#pragma once

#include "game/effects/modeleffectpool.h"

namespace modeleffects {
    class IcicleOwner final : public Owner {
    public:
        IcicleOwner();
        void reset() override;
        void setMatrix(const j3dmodel::Matrix& matrix) override;
        void setFrame(double frame) override;
        std::int32_t textureFrame = 0;
        std::shared_ptr<j3dmodel::Animation> textureAnimation;
    };

    class IciclePool final : public Pool {
    public:
        IciclePool(unsigned count, unsigned variant, std::int32_t lifetime, unsigned type);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        void advance(Owner& owner, bool visible) override;

    private:
        unsigned variant;
    };
}
