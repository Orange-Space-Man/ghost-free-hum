#include "../recovered/moneyscatterinitializer.h"

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

    bool near(const Vector& first, const Vector& second) {
        for (unsigned i = 0; i < 3; ++i)
            if (std::abs(first[i] - second[i]) > 0.0001f) return false;
        return true;
    }

    struct Initialization final : moneyscatter::InitializationServices {
        std::vector<std::uint32_t> draws;
        std::vector<unsigned> calls;
        unsigned consumed = 0, models = 0, rooms = 0;
        Vector queriedPosition{};
        moneyscatter::Request* request = nullptr;

        std::uint32_t random() override {
            calls.push_back(1);
            return draws.at(consumed++);
        }
        std::uint32_t roomAt(const Vector& position, std::uint32_t previousRoom) override {
            check(previousRoom == 0xffffffff, "Initialization queries the room without a previous-room hint.");
            calls.push_back(2);
            queriedPosition = position;
            ++rooms;
            return 7;
        }
        void updateModel(moneyscatter::Particle& particle) override {
            calls.push_back(3);
            ++models;
            check(particle.up == Vector{0, 0, 1} && particle.scale == 1 && particle.rotationPhase == 1,
                "The model sees the initialized orientation, scale and phase.");
            if (request) check(request->particles.count == 0, "Model initialization precedes active-list insertion.");
        }
    };

    struct PoolServices final : moneyscatter::Services {
        PoolServices(const actormodel::InitialPose& pose, Initialization& initialization) : pose(pose), initialization(initialization) {}
        const actormodel::InitialPose& pose;
        Initialization& initialization;
        void initializeParticle(moneyscatter::Request& request, moneyscatter::Particle& particle) override {
            initialization.request = &request;
            moneyscatter::initialize(request, particle, pose, initialization);
        }
        std::uint32_t roomAt(const Vector&, std::uint32_t) override { return 7; }
        std::uint16_t roomFlags(std::uint32_t) override { return 1; }
        bool advanceParticle(moneyscatter::Pool&, moneyscatter::Request&, moneyscatter::Particle&) override { return true; }
        void updateAuxiliaryModel() override {}
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the locally owned executable for its original angle tables.");
        std::ifstream stream(argv[1], std::ios::binary);
        check(bool(stream), "Cannot open the local executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const actormodel::InitialPose pose(executable);
        moneyscatter::Request request;
        request.position = {10, 20, 30};
        request.direction = {0, 0, 1};
        request.speed = 4;
        request.lifetime = 150;
        {
            Initialization services;
            services.draws.assign(4, 0x80000000);
            moneyscatter::Particle particle;
            particle.position = {90, 80, 70};
            particle.angularVelocity = -123;
            particle.visible = true;
            particle.blinking = true;
            particle.blinkCounter = 9;
            moneyscatter::initialize(request, particle, pose, services);
            check(services.queriedPosition == Vector{90, 80, 70} && particle.position == request.position,
                "Room lookup must observe the recycled position, not the new spawn position.");
            check(particle.room == 7 && particle.currentRoom == 7 && particle.angularVelocity == -123 && particle.visible,
                "Room fields are assigned while angular velocity and visibility survive reuse.");
            check(near(particle.forward, Vector{0, 0, 1}) && near(particle.velocity, Vector{0, 0, 4}) &&
                particle.angles == std::array<std::int16_t, 3>{}, "Zero jitter preserves forward direction and Euler orientation.");
            check(particle.spin == 180 && particle.sparkleCounter == 40 && particle.lifetime == 150 &&
                !particle.blinking && particle.blinkCounter == 0 && !particle.placedMotion && particle.motion == 0,
                "Scatter initialization restores its original lifetime, blink and motion fields.");
            check(services.calls == std::vector<unsigned>{1, 1, 1, 2, 1, 3}, "Initialization service order changed.");
        }
        for (auto draw : {0u, 0xffffffffu}) {
            Initialization services;
            services.draws = {draw, draw, 0, 0};
            moneyscatter::Particle particle;
            moneyscatter::initialize(request, particle, pose, services);
            const int offset = draw == 0 ? 13107 : -13107;
            const auto yaw = pose.horizontalDirection(static_cast<std::uint16_t>(offset));
            const auto pitch = pose.horizontalDirection(static_cast<std::uint16_t>(16384 + offset));
            check(particle.forward == Vector{pitch[0] * yaw[0], pitch[2], pitch[0] * yaw[2]},
                "Extreme jitter must wrap the original angle table in both directions.");
            for (unsigned i = 0; i < 3; ++i)
                check(particle.velocity[i] == particle.forward[i] * 4, "Velocity must use the unnormalized scatter direction.");
        }
        for (float speed : {0.0f, -2.0f}) {
            Initialization services;
            services.draws = {0x80000000, 0x80000000, 0, 0, 0x80000000, 0xffffffff, 0xffffffff};
            request.positionMode = 1;
            request.speed = speed;
            moneyscatter::Particle particle;
            particle.angles = {11, 22, 33};
            moneyscatter::initialize(request, particle, pose, services);
            check(particle.position == Vector{60, 20, 0} && services.consumed == 7,
                "Position spread consumes all three draws, including the zero-height spread.");
            check(particle.angles == std::array<std::int16_t, 3>{11, 22, 33}, "Nonpositive speed retains Euler angles.");
            check(particle.sparkleCounter == 80, "Unsigned random conversion can round its maximum to one.");
        }
        {
            request.lifetime = 1000;
            request.positionMode = 0;
            request.speed = 0;
            Initialization services;
            services.draws = {0, 0x80000000, 0};
            moneyscatter::Particle particle;
            particle.room = 9;
            particle.currentRoom = 10;
            bool rejected = false;
            try { moneyscatter::initialize(request, particle, pose, services); }
            catch (const std::runtime_error&) { rejected = true; }
            check(rejected && services.consumed == 0, "Unknown original stack contents must not be silently replaced.");
            moneyscatter::initialize(request, particle, pose, services, Vector{3, 4, 5});
            check(particle.motion == 3 && particle.placedMotion && particle.spin == 90 &&
                particle.forward == Vector{0, 0, 1} && particle.velocity == Vector{}, "Placed-money branch changed.");
            check(particle.room == 9 && particle.currentRoom == 10 && services.rooms == 0 && services.consumed == 3,
                "Placed money retains room ownership and consumes three draws.");
            services.draws = {0, 0xffffffff, 0};
            services.consumed = 0;
            request.speed = 2;
            moneyscatter::initialize(request, particle, pose, services, Vector{0, 0, 1});
            check(particle.spin == 270 && particle.velocity == Vector{0, 0, 2},
                "Placed money selects the other face above half range and uses supplied scratch for velocity.");
        }
        {
            Initialization services;
            services.draws.assign(8, 0x80000000);
            PoolServices bindings(pose, services);
            moneyscatter::Pool pool({30, 5, 2});
            pool.particles.at(199).position = {40, 50, 60};
            auto* first = pool.scatter({10, 20, 30}, {0, 0, 1}, 1, 150, 4, 0, bindings);
            auto* particle = first->particles.sentinel.next;
            check(particle == &pool.particles.at(199) && services.queriedPosition == Vector{40, 50, 60},
                "The initializer runs through the original tail allocation route.");
            pool.recycle(*first, *particle);
            auto* second = pool.scatter({70, 80, 90}, {0, 0, 1}, 1, 150, 4, 0, bindings);
            check(second->particles.sentinel.next == particle && services.queriedPosition == Vector{10, 20, 30} &&
                particle->position == Vector{70, 80, 90} && services.models == 2,
                "Reallocation reuses the particle and observes its previous spawn position.");
        }
        std::cout << "Money particle initialization checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

