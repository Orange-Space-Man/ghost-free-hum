#include "../recovered/moneyscattercontact.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using moneyscatter::Vector;

    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    bool near(float first, float second) { return std::abs(first - second) < 0.0001f; }

    struct Services final : moneyscatter::ContactServices {
        mapcollision::SphereContact contact;
        Vector player{10, 20, 30}, queryPosition{}, queryVelocity{}, effectPosition{};
        float top = 100, queryRadius = 0;
        std::uint32_t flags = 0, queriedRoom = 0;
        std::vector<unsigned> calls;
        unsigned events = 0;
        moneyscatter::Pool* pool = nullptr;
        moneyscatter::Request* request = nullptr;
        mapcollision::SphereContact sweep(const Vector& position, float radius, const Vector& velocity) override {
            queryPosition = position;
            queryVelocity = velocity;
            queryRadius = radius;
            return contact;
        }
        Vector& playerPosition() override { return player; }
        float playerTop() override { return top; }
        void playerEvent(unsigned event, const playerevents::Payload& payload) override {
            if (pool) {
                check(event == 0x13 && payload.word(0x60) == 5 && payload.word(0x64) == 1,
                    "Suction dispatches the original coin type/count payload.");
                check(pool->particles.free.count == 200 && request->particles.count == 0,
                    "Suction recycles before dispatching the player event.");
            } else {
                check(event == 0x26 && payload.word(0x94) == 1 && payload.word(0x28) == 5 && payload.word(0x30) == 0xffffffff,
                    "Contact collection requires the original initialized payload with one coin.");
            }
            ++events;
            calls.push_back(1);
        }
        void collectionEffect(const Vector& position) override { effectPosition = position; calls.push_back(2); }
        std::uint32_t roomFlags(std::uint32_t room) override { queriedRoom = room; calls.push_back(3); return flags; }
        void notifyPlacedCollection(std::uint32_t) override { calls.push_back(4); }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the locally owned executable for original angle tables.");
        std::ifstream stream(argv[1], std::ios::binary);
        check(bool(stream), "Cannot open the local executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const actormodel::InitialPose pose(executable);
        {
            moneyscatter::Particle particle;
            particle.position = {1, 2, 3};
            particle.velocity = {10, 0, -10};
            particle.acceleration = {1, 0.5f, -1};
            particle.scale = 0.5f;
            moneyscatter::move(particle, true);
            check(near(particle.velocity[0], 10.9f) && particle.velocity[1] == -2 && near(particle.velocity[2], -10.9f) &&
                near(particle.position[0], 11.9f) && particle.position[1] == 0 && near(particle.position[2], -7.9f) &&
                near(particle.scale, 0.575f), "Drag, acceleration, gravity, integration and scale recovery changed.");
            particle.motion = 2;
            const auto position = particle.position;
            moneyscatter::move(particle, false);
            check(particle.position == position && near(particle.scale, 0.575f), "Settling particles do not integrate motion.");
            particle.motion = 0;
            particle.placedMotion = true;
            particle.lifetime = 1000;
            particle.velocity = {0, -20, 0};
            moneyscatter::move(particle, false);
            check(particle.velocity[1] == -5 && particle.scale == 1 && !particle.placedMotion,
                "Released placed money caps its first falling speed and restores scale once.");
            moneyscatter::move(particle, false);
            check(near(particle.velocity[1], -6.95f), "Placed-money fall cap must not repeat.");
        }
        for (float speed : {2.5f, 2.5001f, 5.0f, 5.001f, 20.0f, 20.001f}) {
            Services services;
            services.contact.floor = 0;
            services.contact.floorNormal = {0, 1, 0};
            services.contact.correction = {1, 2, 3};
            moneyscatter::Particle particle;
            particle.velocity = {0, -speed, 0};
            particle.acceleration = {1, 2, 3};
            particle.forward = {0, 0, 1};
            particle.spin = 45;
            particle.scale = 0.5f;
            const auto result = moneyscatter::collide(particle, true, pose, services);
            check(services.queryRadius == 10 && services.queryVelocity == Vector{0, -speed, 0} &&
                particle.position == Vector{1, 2, 3}, "Collision uses scaled radius, incoming velocity and correction.");
            check(result.audible == (speed > 5) && !result.wall, "Bounce audio threshold must remain strict.");
            check(particle.motion == (speed <= 2.5f ? 2 : speed <= 20 ? 1 : 0), "Settling/rolling thresholds changed.");
            if (speed <= 2.5f) {
                check(particle.velocity == Vector{} && particle.acceleration == Vector{} && particle.angularVelocity == 3908,
                    "Settling clears forces and converts spin/speed to retained angular velocity.");
            } else check(near(particle.velocity[1], speed * 0.75f), "Floor reflection must retain original damping.");
        }
        {
            Services services;
            services.contact.floor = 0;
            services.contact.wall = 1;
            services.contact.floorNormal = {0, 1, 0};
            services.contact.wallNormal = {1, 0, 0};
            moneyscatter::Particle particle;
            particle.velocity = {8, -8, 0};
            auto result = moneyscatter::collide(particle, true, pose, services);
            check(particle.velocity == Vector{-4.5f, 4.5f, 0} && result.wall && result.audible,
                "Simultaneous contact reflects floor before wall with damping on each reflection.");
            particle.velocity = {8, -8, 0};
            particle.motion = 0;
            result = moneyscatter::collide(particle, false, pose, services);
            check(particle.velocity == Vector{-6, -6, 0} && particle.motion == 0 && result.wall,
                "Suction suppresses floor response but retains wall response.");
            services.contact = {};
            particle.velocity = {8, -8, 0};
            result = moneyscatter::collide(particle, true, pose, services);
            check(!result.audible && !result.wall && particle.velocity == Vector{8, -8, 0}, "No contact leaves velocity intact.");
        }
        for (float spin : {90.0f, 270.0f, -1.0f, 361.0f}) {
            Services services;
            services.contact.floor = 0;
            moneyscatter::Particle particle;
            particle.forward = {0, 0, 1};
            particle.spin = spin;
            bool rejected = false;
            try { moneyscatter::collide(particle, true, pose, services); }
            catch (const std::runtime_error&) { rejected = true; }
            check(rejected, "Undefined folded-spin paths need the original incoming register, not a fabricated value.");
            moneyscatter::collide(particle, true, pose, services, 12.0);
            check(particle.motion == 2 && particle.angularVelocity == 3032, "Explicit retained folded spin must be used.");
        }
        for (bool wall : {false, true}) {
            Services services;
            moneyscatter::Request request;
            request.placedMoney = true;
            moneyscatter::Particle particle;
            particle.position = {40, 50, 70};
            particle.currentRoom = 0x03001234;
            particle.lifetime = 150;
            request.particles.append(particle);
            check(moneyscatter::collect(request, particle, wall, services), "Coin inside the player cylinder must collect.");
            check(particle.lifetime == 0 && request.particles.count == 1 && services.effectPosition == Vector{40, 50, 70} &&
                services.queriedRoom == 0x03001234 && services.calls == std::vector<unsigned>{1, 2, 3, 4},
                "Collection dispatches, emits, expires and notifies without prematurely recycling.");
            if (wall) check(near(services.player[0], 4) && near(services.player[2], 22) && particle.position == Vector{40, 50, 70},
                "Wall contact displaces the player rather than the coin.");
            else check(near(particle.position[0], 46) && near(particle.position[2], 78) && services.player == Vector{10, 20, 30},
                "Ordinary contact displaces the coin to the collection radius.");
        }
        for (const auto position : {Vector{70, 20, 30}, Vector{70, 100, 30}, Vector{70.001f, 20, 30},
            Vector{10, 19.999f, 30}, Vector{10, 100.001f, 30}}) {
            Services services;
            moneyscatter::Request request;
            moneyscatter::Particle particle;
            particle.position = position;
            const bool expected = position[0] == 70;
            check(moneyscatter::collect(request, particle, false, services) == expected && services.events == unsigned(expected),
                "Collection radius and vertical limits must be inclusive and reject positions beyond them.");
        }
        {
            Services services;
            services.flags = 0x40;
            moneyscatter::Request request;
            request.placedMoney = true;
            moneyscatter::Particle particle;
            particle.position = services.player;
            check(moneyscatter::collect(request, particle, false, services) && near(particle.position[0], 10.0006f) &&
                particle.position[2] == 30 && services.calls == std::vector<unsigned>{1, 2, 3},
                "Coincident collection retains the tiny unnormalized offset and room suppression.");
        }
        for (auto flags : {0u, 0x40u}) {
            Services services;
            services.flags = flags;
            moneyscatter::Pool pool({30, 5, 2});
            auto& request = pool.persistent();
            auto* particle = pool.particles.free.take();
            particle->currentRoom = 0x04000001;
            particle->lifetime = 123;
            request.particles.append(*particle);
            services.pool = &pool;
            services.request = &request;
            moneyscatter::collectSuction(pool, request, *particle, services);
            check(services.events == 1 && services.queriedRoom == 0x04000001 && particle->lifetime == 123 &&
                pool.active.count == 1 && services.calls == (flags ? std::vector<unsigned>{1, 3} : std::vector<unsigned>{1, 3, 4}),
                "Suction preserves particle fields, retains request ownership and honors placed-room notification gating.");
        }
        std::cout << "Money motion and contact checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

