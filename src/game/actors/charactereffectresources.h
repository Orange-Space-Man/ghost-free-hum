#pragma once

#include "game/effects/modeleffectpool.h"
#include "game/actors/actorregistry.h"

namespace charactereffects {
    class Resources final : public characterownership::Resources {
    public:
        Resources(modeleffects::Factory& models, particles::EffectFactory& particles, const unsigned& variant);
        Resources(particles::EffectFactory& particles, const unsigned& variant);
        void bindModels(modeleffects::Factory& models);
        void bind(enemies::Manager& manager, actors::Registry& actors);
        characterownership::Resource* createPrimary(unsigned type, const std::array<float, 3>& position) override;
        characterownership::Resource* createSecondary(unsigned type, const std::array<float, 3>& position,
            const std::array<float, 3>& vector) override;
        std::int32_t playerValue() override;
        unsigned gameVariant() override;

    private:
        modeleffects::Factory* models = nullptr;
        particles::EffectFactory& particles;
        const unsigned& variant;
        enemies::Manager* manager = nullptr;
        actors::Registry* actors = nullptr;
    };
}
