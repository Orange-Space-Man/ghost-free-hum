#include "game/enemies/itemsuctionforce.h"
#include "game/player/playervacuummodel.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    struct Volume : playermodels::VacuumVolume {
        mutable unsigned calls = 0;
        bool hit = true;
        bool intersects(double radius, itemsuction::Vector& direction, const itemsuction::Vector&) const override {
            require(radius == 200, "Item suction must retain its original query radius.");
            ++calls;
            direction = {0, 0, -1};
            return hit;
        }
    };
}

int main() {
    try {
        using Vector = itemsuction::Vector;
        playeractor::View player{};
        Volume volume;
        Vector origin{}, axis{0, 0, 1};
        particles::SuctionContext context{&origin, &axis, 1};
        itemsuction::Force force(player, context);
        itemsuction::Parameters parameters;
        Vector velocity{1, 2, 3}, acceleration{4, 5, 6};
        std::uint8_t remaining = 7, duration = 8;
        require(force.apply(25, {0, 0, 100}, velocity, acceleration, remaining, duration, true, parameters, {}) == 0 &&
            acceleration == Vector{} && velocity == Vector{1, 2, 3} && remaining == 7 && duration == 0,
            "Inactive suction clears acceleration and duration while preserving velocity and remaining count.");
        player.field8A0 = 1;
        player.field8B4 = &volume;
        duration = 255;
        acceleration = {4, 5, 6};
        require(force.apply(25, {0, 0, 25}, velocity, acceleration, remaining, duration, true, parameters, {}) == 2 &&
            duration == 0 && acceleration == Vector{4, 5, 6} && volume.calls == 0,
            "Arrival includes its radius boundary and does not overwrite force outputs or query the volume.");
        duration = 255;
        acceleration = {};
        require(force.apply(25, {0, 0, 100}, velocity, acceleration, remaining, duration, false, parameters, {}) == 1 &&
            std::abs(velocity[2] + 15) < 0.0001f && acceleration == Vector{} && duration == 255,
            "Axis suction sets capped velocity and preserves non-countdown capture state.");
        duration = remaining = 10;
        force.apply(25, {0, 0, 100}, velocity, acceleration, remaining, duration, true, parameters, {});
        require(remaining == 9 && std::abs(velocity[2] - 9.855f) < 0.0001f,
            "Capture countdown applies the original opposing velocity after force calculation.");
        duration = 0;
        parameters.durationScale = 26000;
        parameters.durationBase = 5;
        force.apply(25, {0, 0, 100}, velocity, acceleration, remaining, duration, true, parameters, {});
        require(duration == 9 && remaining == 9, "Original ineffective countdown clamp wraps 265 to byte nine.");
        const auto calls = volume.calls;
        acceleration = {1, 2, 3};
        require(force.apply(25, {0, 0, -100}, velocity, acceleration, remaining, duration, true, parameters, {}) == 0 &&
            volume.calls == calls && acceleration == Vector{} && duration == 0,
            "Facing rejection precedes the vacuum volume query.");
        volume.hit = false;
        duration = 33;
        require(force.apply(25, {0, 0, 100}, velocity, acceleration, remaining, duration, false, parameters, {}) == 0 && duration == 33,
            "A missed non-countdown query retains capture duration.");
        volume.hit = true;
        duration = 0;
        parameters.durationScale = 0;
        parameters.durationBase = 0;
        force.apply(25, {0, 0, 10}, velocity, acceleration, remaining, duration, true, parameters, {});
        require(duration == 0 && force.apply(25, {0, 0, 10}, velocity, acceleration, remaining, duration, true, parameters, {}) == 1,
            "Zero-duration capture must continue querying inside the arrival radius.");
        duration = 255;
        acceleration = {};
        force.apply(25, {10, 0, 100}, velocity, acceleration, remaining, duration, false, parameters, {});
        const float distance = std::sqrt(10100.0f);
        const float expected = -(800.0f / distance) * (1 - (1.0f / distance) * 24);
        require(std::abs(acceleration[0] - expected) < 0.0001f && acceleration[1] == 0 && acceleration[2] == 0,
            "Off-axis suction applies transverse pull and distance-dependent drag.");
        std::cout << "Item suction force checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
