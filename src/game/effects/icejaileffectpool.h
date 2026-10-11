#pragma once

#include "game/effects/modeleffectpool.h"

namespace modeleffects {
    class IceJailOwner final : public Owner {
    public:
        IceJailOwner();
        void reset() override;
        void setFrame(double frame) override;
        float colorFrame = 0;
    };

    class IceJailPool final : public Pool {
    public:
        IceJailPool();
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> colorAnimation, textureAnimation;
    };
}
