#pragma once

#include "game/effects/modeleffectpool.h"
#include "game/events/runtimeflags.h"

#include <functional>

namespace modeleffects {
    class WaterfallOwner final : public Owner {
    public:
        WaterfallOwner();
        void reset() override;
        void setFrame(double frame) override;
        std::uint8_t fade();
        std::array<particles::Emitter*, 2> emitters{};
        std::array<float, 3> position{};
        std::int32_t soundFrame = 0;
    };

    class WaterfallPool final : public Pool {
    public:
        using SoundEmitter = std::function<void(unsigned, const std::array<float, 3>&, unsigned)>;
        WaterfallPool(particles::ArchiveResources& resources, particles::EmitterPool& emitters,
            const gameflags::State& flags, SoundEmitter sound);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available() override;
        Owner* available(const j3dmodel::Matrix& matrix) override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> textureAnimation;

    private:
        particles::ArchiveResources& resources;
        particles::EmitterPool& emitters;
        const gameflags::State& flags;
        SoundEmitter sound;
    };
}
