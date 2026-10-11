#pragma once

#include "game/effects/modeleffectpool.h"

#include <functional>

namespace modeleffects {
    class CrystalOwner final : public Owner {
    public:
        CrystalOwner();
        void reset() override;
        void release() override;
        void setFrame(double frame) override;
        std::array<float, 3> position{};
    };

    class CrystalPool final : public Pool {
    public:
        using Sound = std::function<void(unsigned, const std::array<float, 3>&, unsigned)>;
        using Scratch = std::function<std::array<std::uint8_t, 3>()>;
        CrystalPool(particles::EffectFactory& particles, std::uint32_t& randomState, Sound sound, Scratch scratch);
        void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) override;
        Owner* available(const j3dmodel::Matrix& matrix) override;
        void advance(Owner& owner, bool visible) override;
        std::shared_ptr<j3dmodel::Animation> colorAnimation, textureAnimation;

    private:
        particles::EffectFactory& particles;
        std::uint32_t& randomState;
        Sound sound;
        Scratch scratch;
    };
}
