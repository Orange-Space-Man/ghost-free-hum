#include "game/effects/particlesuction.h"
#include "game/player/playervacuummodel.h"
#include "game/effects/particlechildren.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    bool near(float first, float second) { return std::abs(first - second) < 0.0001f; }

    struct Volume : playermodels::VacuumVolume {
        bool hit = true;
        mutable unsigned calls = 0;
        bool intersects(double radius, Vector& direction, const Vector&) const override {
            require(radius == 5.5, "Particle attraction retains the original query radius.");
            ++calls;
            if (hit) direction = {0, 0, -1};
            return hit;
        }
    };
}

int main() {
    try {
        {
            particles::Emitter emitter;
            particles::Pool<particles::ShortNode> pool(2);
            emitter.shortNodePool = &pool.free;
            particles::Node parent;
            parent.fields0.fill(0);
            parent.fields0[0] = 10;
            parent.fields24 = {0x3f800000, 0x40000000, 0x40400000};
            parent.fields30 = {10, 0};
            parent.fields38 = {2, 3, 0};
            parent.fields54 = {11, 22, 33, 200, 0};
            emitter.nodes.append(parent);
            emitter.parameters.fields150 = {0x40000000, 0x3f000000, 0, 0x123456ff};
            emitter.parameters.fields160 = {2, 1};
            std::uint32_t random = 1, expected = random;
            for (unsigned i = 0; i < 3; ++i) expected = expected * 0x5eedf715u + 0x1b0cb173u;
            particles::updateChildren(emitter, random);
            auto* child = emitter.shortNodes.sentinel.next;
            require(random == expected && emitter.shortNodes.count == 1 && pool.free.count == 1 &&
                child->fields0 == std::array<float, 6>{10, 0, 0, 1, 2, 3} &&
                child->fields18 == std::array<std::uint8_t, 2>{2, 1} &&
                child->fields20 == std::array<std::uint8_t, 4>{0x12, 0x34, 0x56, 127} &&
                child->fields1A[2] == 0x41 && child->fields1A[3] == 0x40 && child->fields1A[4] == 0 && child->fields1A[5] == 0,
                "Child creation consumes three draws even at zero spread, copies parent offset and ages on the same tick.");
            emitter.parameters.flags = 0x4000;
            particles::updateChildren(emitter, random);
            auto* second = emitter.shortNodes.sentinel.next;
            require(second != child && emitter.shortNodes.count == 1 && pool.free.count == 1 &&
                second->fields20 == std::array<std::uint8_t, 4>{11, 22, 33, 100} &&
                child->fields0 == std::array<float, 6>{} && child->fields18 == std::array<std::uint8_t, 2>{} &&
                child->fields1A[2] == 0x41,
                "Retired children return to the shared pool while retaining fields the original reset leaves untouched.");
            emitter.parameters.fields160[1] = 2;
            const auto unchanged = random;
            particles::updateChildren(emitter, random);
            require(emitter.shortNodes.count == 0 && pool.free.count == 2 && random == unchanged,
                "Skipped emission consumes no random values but still retires existing children.");
            emitter.parameters.fields160 = {1, 1};
            particles::updateChildren(emitter, random);
            require(emitter.shortNodes.count == 0 && pool.free.count == 2,
                "A lifetime-one child is created and retired during the same original update.");
            emitter.parameters.fields160 = {9, 1};
            particles::updateChildren(emitter, random);
            particles::updateChildren(emitter, random);
            const auto fullRandom = random;
            particles::updateChildren(emitter, random);
            require(!pool.free.count && random == fullRandom && emitter.shortNodes.count == 2,
                "Pool exhaustion stops child creation without consuming RNG and still advances existing lifetimes.");
        }
        Vector origin{}, axis{0, 0, 1};
        particles::SuctionContext context{&origin, &axis, 1};
        playeractor::View player{};
        Volume volume;
        unsigned draws = 0;
        particles::Suction suction(player, context, [&] { ++draws; return 0u; });
        Vector velocity{7, 8, 9}, acceleration{1, 2, 3};
        std::uint8_t captured = 1;
        require(suction.apply(1000, 4, 18, {0, 0, 100}, velocity, acceleration, captured) == 0 &&
            acceleration == Vector{} && velocity == Vector{7, 8, 9} && !captured && !draws,
            "Inactive suction clears acceleration and capture but preserves velocity without consuming RNG.");
        player.field8A0 = 1;
        require(suction.apply(1000, 4, 18, {0, 0, 100}, velocity, acceleration, captured) == 0 && !draws,
            "A missing volume disables particle attraction.");
        player.field8B4 = &volume;
        acceleration = {1, 2, 3};
        require(suction.apply(1000, 4, 18, {0, 0, 20}, velocity, acceleration, captured) == 2 &&
            acceleration == Vector{1, 2, 3} && !volume.calls && !draws,
            "The inclusive twenty-unit completion boundary preserves acceleration and bypasses volume and RNG.");
        require(suction.apply(1000, 4, 18, {0, 0, 100}, velocity, acceleration, captured) == 1 && captured == 1 &&
            draws == 1 && near(velocity[2], -18) && acceleration == Vector{1, 2, -7},
            "Captured particles accumulate force and clamp velocity, consuming exactly one random draw.");
        volume.hit = false;
        acceleration = {};
        require(suction.apply(1000, 4, 18, {0, 0, 100}, velocity, acceleration, captured) == 1 &&
            captured == 1 && draws == 2 && near(acceleration[2], -10),
            "Previously captured particles remain attracted after leaving the vacuum volume.");
        suction.retainCapture = false;
        require(suction.apply(1000, 4, 18, {0, 0, 100}, velocity, acceleration, captured) == 0 && !captured &&
            draws == 2 && acceleration == Vector{}, "Disabling retention releases a missed particle without RNG.");
        volume.hit = true;
        acceleration = {};
        suction.apply(1000, 4, 18, {100, 0, 100}, velocity, acceleration, captured);
        require(acceleration[0] < -7 && near(acceleration[2], 0) && draws == 3,
            "Off-axis particles accelerate toward the vacuum axis while velocity follows the query direction.");
        particles::Node node;
        node.fields0.fill(0);
        node.fields24.fill(0);
        node.fields54.fill(0);
        node.fields30 = {50, 0};
        node.fields0[2] = 100;
        particles::SuctionCallback callback(suction);
        callback.capture = 1;
        require(callback.advance(node) && node.fields54[4] == 1 && node.fields30[1] == 65535,
            "Capture callbacks preserve unsigned life-counter underflow.");
        player.field8A0 = 0;
        callback.advance(node);
        require(node.fields54[4] == 1 && node.fields0[6] == 0 && node.fields0[7] == 0 && node.fields0[8] == 0,
            "A previously latched particle flag survives loss of suction while acceleration clears.");
        player.field8A0 = 1;
        node.fields0[2] = 20;
        callback.advance(node);
        require(node.fields30[1] == 50 && node.fields54[4] == 1,
            "Arrival copies the lifetime limit and retains the original latched flag.");
        callback.capture = 0;
        node.fields54[4] = 0;
        node.fields0[2] = 100;
        callback.advance(node);
        require(node.fields30[1] == 50 && node.fields54[4] == 0,
            "Damage-effect callbacks apply force without enabling the capture lifetime bookkeeping.");
        const auto calls = volume.calls;
        require(suction.apply(1000, 4, 18, {std::numeric_limits<float>::quiet_NaN(), 0, 100},
            velocity, acceleration, captured) == 1 && volume.calls == calls + 1 && std::isnan(acceleration[0]),
            "Unordered distance continues through the original query and force branches rather than completing capture.");
        std::cout << "Particle suction checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
