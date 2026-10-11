#pragma once

#include "game/effects/modeleffectpool.h"

namespace modeleffects {
    class BridgeOwner final : public Owner {
    public:
        BridgeOwner();
        void reset() override;
        void setFrame(double frame) override;
    };

    class BridgePool final : public Pool {
    public:
        BridgePool();
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> colorAnimation, textureAnimation;
    };
}
