#pragma once

#include "billscatterpool.h"
#include "leafscatterinitializer.h"

namespace leafscatter {
    class Owner {
    public:
        void initialize(const std::shared_ptr<j3dmodel::ModelData>& model);
        void release();
        billscatter::Request* scatter(const billscatter::Vector& position, const billscatter::Vector& direction,
            double strength, std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
            const actormodel::InitialPose& pose, InitializationServices& services);
        void update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold, billscatter::Services& services);
        bool active() const;
        billscatter::Pool* storage();
        const std::array<std::uint16_t, 3>& color() const;

    private:
        std::unique_ptr<billscatter::Pool> pool;
        std::array<std::uint16_t, 3> channels{};
    };
}
