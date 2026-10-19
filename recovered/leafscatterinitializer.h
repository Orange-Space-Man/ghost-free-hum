#pragma once

#include "billscatterpool.h"
#include "game/actors/actorinitialpose.h"

namespace leafscatter {
    struct InitializationServices {
        virtual ~InitializationServices() = default;
        virtual std::uint32_t random() = 0;
        virtual std::uint32_t roomAt(const billscatter::Vector& position, std::uint32_t previous) = 0;
        virtual std::uintptr_t startSound(unsigned id, const billscatter::Vector& position) = 0;
        virtual void soundCount(std::uintptr_t handle, std::uint8_t count) = 0;
    };

    billscatter::Request* scatter(billscatter::Pool& pool, const billscatter::Vector& position,
        const billscatter::Vector& direction, double strength, std::uint8_t delay, std::uint8_t count,
        std::uint16_t lifetime, const actormodel::InitialPose& pose, InitializationServices& services);
}
