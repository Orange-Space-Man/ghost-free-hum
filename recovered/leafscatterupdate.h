#pragma once

#include "billscatterupdate.h"

namespace leafscatter {
    struct UpdateServices : billscatter::ContactServices {
        virtual int suction(billscatter::Particle& particle) = 0;
        virtual billscatter::Vector suctionPosition() = 0;
        virtual billscatter::Vector playerPosition() = 0;
        virtual float playerTop() = 0;
    };

    void writeColor(j3dmodel::ModelInstance& model, const std::array<std::uint16_t, 4>& color);
    bool advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle,
        const std::array<std::uint16_t, 3>& color, float flutterStrength, std::uint32_t& particleRandom,
        std::uint32_t& gameplayRandom, const actormodel::InitialPose& pose, UpdateServices& services);
}
