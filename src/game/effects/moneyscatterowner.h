#pragma once

#include "moneyscatterscenebindings.h"
#include "moneyscattermodels.h"

namespace moneyscatter {
    class Owner final : private Services {
    public:
        Owner(Timing timing, const std::shared_ptr<j3dmodel::ModelData>& coin,
            const actormodel::InitialPose& pose, SceneDependencies dependencies, float retainedAuxiliarySpin);
        Owner(Timing timing, const std::shared_ptr<j3dmodel::ModelData>& coin,
            const actormodel::InitialPose& pose, SceneDependencies dependencies, RetainedState& retained);
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        Request* scatter(const Vector& position, const Vector& direction, std::uint8_t count,
            std::uint16_t lifetime, double speed, std::uint8_t positionMode);
        void updateCoins(const Vector& cameraPosition, const Vector& cameraDirection);
        bool place(const Vector& position, std::int32_t row, std::uint32_t room, const Vector& retainedDirection);
        Pool& storage();
        const Models& modelStorage() const;

    private:
        void initializeParticle(Request& request, Particle& particle) override;
        std::uint32_t roomAt(const Vector& position, std::uint32_t previousRoom) override;
        std::uint16_t roomFlags(std::uint32_t room) override;
        bool advanceParticle(Pool& pool, Request& request, Particle& particle) override;
        void updateAuxiliaryModel() override;
        SceneBindings bindings;
        Pool pool;
        Models models;
        const actormodel::InitialPose& pose;
        particles::Batch& sparkles;
        UpdateState state;
        Vector cameraPosition{}, cameraDirection{};
    };
}
