#pragma once

#include "game/effects/modeleffectpool.h"
#include "game/enemies/enemytypeparameters.h"

namespace modeleffects {
    class SharedOwner final : public Owner {
    public:
        SharedOwner();
        void reset() override;
        void setPosition(const std::array<float, 3>& position) override;
        std::int32_t colorFrame = 0;
        std::uint32_t field14 = 0;
        float scaleFactor;
    };

    class SharedPool final : public Pool {
    public:
        SharedPool();
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> textureAnimation, colorAnimation;
    };

    SharedOwner* createShared(Factory& factory, const enemytypes::Manager& parameters, unsigned type,
        const std::array<float, 3>& position);
}
