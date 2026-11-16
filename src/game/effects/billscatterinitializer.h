#pragma once

#include "billscatterpool.h"

namespace billscatter {
    struct InitializationServices {
        virtual ~InitializationServices() = default;
        virtual std::uint32_t random() = 0;
        virtual std::uint32_t roomAt(const Vector& position, std::uint32_t previous) = 0;
        virtual void updateModel(Particle& particle) = 0;
    };

    void initialize(Request& request, Particle& particle, std::uint8_t flutterThreshold, InitializationServices& services);
    Request* scatter(Pool& pool, const Vector& position, const Vector& direction, double strength,
        std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
        std::uint8_t flutterThreshold, InitializationServices& services);
    void scatter(Owners& owners, const Vector& position, const Vector& direction, double strength,
        std::uint8_t delay, std::uint8_t count, std::uint16_t lifetime,
        std::uint8_t flutterThreshold, InitializationServices& services);
}
