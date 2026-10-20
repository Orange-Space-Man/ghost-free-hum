#pragma once

#include "leafscatterowner.h"
#include "leafscatterupdate.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/effects/particlescenecontext.h"
#include "game/world/roomlocation.h"

namespace leafscatter {
    struct SceneDependencies {
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        const mapcollision::State& collision;
        const billscatter::Vector& playerPosition;
        const particles::SceneContext& playerContext;
        const unsigned& suctionMode;
        const float& suctionRange;
        std::uint32_t& gameplayRandom;
        std::uint32_t& particleRandom;
        const float& flutterStrength;
        const bool& paused;
        const std::uint8_t& blinkThreshold;
        jaudio::PositionalPlayback& audio;
        jaudio::PositionOwners& positions;
    };

    struct RenderServices {
        virtual ~RenderServices() = default;
        virtual void draw(j3dmodel::ModelInstance& model) = 0;
        virtual void view(j3dmodel::ModelInstance& model, const j3dmodel::Matrix& camera) = 0;
    };

    class Scene final : private InitializationServices, private UpdateServices, private billscatter::Services {
    public:
        Scene(const actormodel::InitialPose& pose, SceneDependencies dependencies);
        ~Scene();
        void initialize(const std::shared_ptr<j3dmodel::ModelData>& model);
        void release();
        void scatter(const billscatter::Vector& position, const billscatter::Vector& direction,
            double strength, std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime);
        void update();
        void draw(RenderServices& services);
        void view(const j3dmodel::Matrix& camera, RenderServices& services);
        Owner& storage();

    private:
        std::uint32_t random() override;
        std::uint32_t roomAt(const billscatter::Vector& position, std::uint32_t previous) override;
        std::uintptr_t startSound(unsigned id, const billscatter::Vector& position) override;
        void soundCount(std::uintptr_t handle, std::uint8_t count) override;
        std::uint16_t visibilityFlags(std::uint32_t room) override;
        mapcollision::SphereContact sweep(const billscatter::Vector& position, float radius, const billscatter::Vector& velocity) override;
        int suction(billscatter::Particle& particle) override;
        billscatter::Vector suctionPosition() override;
        billscatter::Vector playerPosition() override;
        float playerTop() override;
        bool advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle) override;
        const actormodel::InitialPose& pose;
        SceneDependencies dependencies;
        Owner owner;
        jaudio::PositionalSound* pendingSound = nullptr;
    };
}
