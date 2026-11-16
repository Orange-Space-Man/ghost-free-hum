#pragma once

#include "moneyscatterupdate.h"
#include "game/effects/particlebatch.h"
#include "game/effects/particlescenecontext.h"
#include "game/world/roomlocation.h"

#include <functional>

namespace moneyscatter {
    struct SceneDependencies {
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        std::vector<std::uint32_t>& roomFlags;
        std::array<std::uint16_t, 256>& savedRoomFlags;
        const unsigned& mapMode;
        const mapcollision::State& collision;
        Vector& playerPosition;
        const particles::SceneContext& playerContext;
        const float& minimumScale;
        std::uint32_t& randomState;
        particles::EffectFactory& effects;
        particles::Batch& sparkles;
        std::function<void(unsigned, const playerevents::Payload&)> dispatch;
        std::function<int(float, Particle&)> attraction;
        std::function<void(unsigned, const Vector&, unsigned)> positionalSound;
    };

    class SceneBindings final : public InitializationServices, public UpdateServices {
    public:
        explicit SceneBindings(SceneDependencies dependencies);
        std::uint32_t random() override;
        std::uint32_t roomAt(const Vector& position, std::uint32_t previousRoom) override;
        std::uint16_t visibilityFlags(std::uint32_t room) const;
        mapcollision::SphereContact sweep(const Vector& position, float radius, const Vector& velocity) override;
        Vector& playerPosition() override;
        float playerTop() override;
        void playerEvent(unsigned event, const playerevents::Payload& payload) override;
        void collectionEffect(const Vector& position) override;
        std::uint32_t roomFlags(std::uint32_t room) override;
        void notifyPlacedCollection(std::uint32_t room) override;
        int suction(float radius, Particle& particle) override;
        Vector suctionPosition() override;
        float suctionMinimumScale() override;
        void bounceSound(const Vector& position) override;
        void sparkle(const Vector& position) override;
        void updateModel(Particle& particle) override;

    private:
        SceneDependencies dependencies;
    };
}
