#pragma once

#include "billscatterinitializer.h"
#include "billscatterupdate.h"
#include "game/effects/particlescenecontext.h"
#include "game/world/roomlocation.h"

#include <functional>

namespace billscatter {
    struct SceneDependencies {
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        const mapcollision::State& collision;
        const Vector& playerPosition;
        const particles::SceneContext& playerContext;
        std::uint32_t& gameplayRandom;
        std::uint32_t& particleRandom;
        const float& flutterStrength;
        const bool& paused;
        const std::uint8_t& blinkThreshold;
        std::function<void(unsigned, const playerevents::Payload&)> dispatch;
        std::function<int(Particle&)> attraction;
    };

    class Scene final : private InitializationServices, private UpdateServices, private Services {
    public:
        Scene(const std::array<std::shared_ptr<j3dmodel::ModelData>, 3>& models,
            const actormodel::InitialPose& pose, SceneDependencies dependencies);
        void scatter(const Vector& position, const Vector& direction, double strength,
            std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime);
        void update();
        Owners& storage();

    private:
        std::uint32_t random() override;
        std::uint32_t roomAt(const Vector& position, std::uint32_t previous) override;
        std::uint16_t visibilityFlags(std::uint32_t room) override;
        mapcollision::SphereContact sweep(const Vector& position, float radius, const Vector& velocity) override;
        Vector playerPosition() override;
        float playerTop() override;
        void playerEvent(unsigned event, const playerevents::Payload& payload) override;
        int suction(Particle& particle) override;
        Vector suctionPosition() override;
        void updateModel(Particle& particle) override;
        bool advance(Pool& pool, Request& request, Particle& particle) override;
        SceneDependencies dependencies;
        const actormodel::InitialPose& pose;
        Owners owners;
    };
}
