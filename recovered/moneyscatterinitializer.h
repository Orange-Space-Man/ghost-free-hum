#pragma once

#include "moneyscatterpool.h"
#include "game/actors/actorinitialpose.h"

#include <optional>

namespace moneyscatter {
    std::array<std::int16_t, 3> directionAngles(const Vector& direction, const actormodel::InitialPose& pose);

    struct InitializationServices {
        virtual ~InitializationServices() = default;
        virtual std::uint32_t random() = 0;
        virtual std::uint32_t roomAt(const Vector& position, std::uint32_t previousRoom) = 0;
        virtual void updateModel(Particle& particle) = 0;
    };

    void initialize(const Request& request, Particle& particle, const actormodel::InitialPose& pose,
        InitializationServices& services, const std::optional<Vector>& retainedDirection = std::nullopt);
}
