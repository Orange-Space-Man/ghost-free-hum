#include "../recovered/moneyscatterupdate.h"
#include "../recovered/moneyscattermodels.h"

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

    struct TestServices final : moneyscatter::UpdateServices, moneyscatter::InitializationServices {
        std::vector<unsigned> calls;
        mapcollision::SphereContact contact;
        Vector player{1000, 0, 1000}, origin{}, queriedVelocity{};
        int suctionResult = 0;
        unsigned modelUpdates = 0, auxiliaryUpdates = 0;
        bool visibleRoom = true;
        moneyscatter::Pool* pool = nullptr;
        moneyscatter::Request* request = nullptr;
        std::uint32_t random() override { return 0x80000000; }
        std::uint32_t roomAt(const Vector&, std::uint32_t) override { return 7; }
        mapcollision::SphereContact sweep(const Vector&, float, const Vector& velocity) override {
            calls.push_back(2);
            queriedVelocity = velocity;
            return contact;
        }
        Vector& playerPosition() override { return player; }
        float playerTop() override { return player[1] + 100; }
        void playerEvent(unsigned event, const playerevents::Payload& payload) override {
            calls.push_back(event);
            check(event == 0x13 || event == 0x26, "Unexpected collection event.");
            if (event == 0x13) check(payload.word(0x60) == 5 && payload.word(0x64) == 1 && request->particles.count == 0,
                "Suction collection must dispatch after recycling.");
            else check(payload.word(0x94) == 1 && request->particles.count == 1,
                "Contact collection must dispatch before recycling.");
        }
        void collectionEffect(const Vector&) override { calls.push_back(3); }
        std::uint32_t roomFlags(std::uint32_t) override { return visibleRoom ? 1 : 0; }
        void notifyPlacedCollection(std::uint32_t) override { calls.push_back(4); }
        int suction(float radius, moneyscatter::Particle&) override {
            check(radius == 100, "Money suction radius changed.");
            calls.push_back(1);
            return suctionResult;
        }
        Vector suctionPosition() override { return origin; }
        float suctionMinimumScale() override { return 0.2f; }
        void bounceSound(const Vector&) override { calls.push_back(5); }
        void sparkle(const Vector&) override { calls.push_back(6); }
        void updateModel(moneyscatter::Particle& particle) override {
            calls.push_back(7);
            ++modelUpdates;
            if (particle.model) moneyscatter::updateModel(particle);
        }
    };

    struct Bindings final : moneyscatter::Services {
        Bindings(const actormodel::InitialPose& pose, TestServices& services) : pose(pose), services(services) {}
        const actormodel::InitialPose& pose;
        TestServices& services;
        moneyscatter::UpdateState state;
        void initializeParticle(moneyscatter::Request& request, moneyscatter::Particle& particle) override {
            moneyscatter::initialize(request, particle, pose, services);
        }
        std::uint32_t roomAt(const Vector& position, std::uint32_t previous) override { return services.roomAt(position, previous); }
        std::uint16_t roomFlags(std::uint32_t room) override { return static_cast<std::uint16_t>(services.roomFlags(room)); }
        bool advanceParticle(moneyscatter::Pool& pool, moneyscatter::Request& request, moneyscatter::Particle& particle) override {
            services.pool = &pool;
            services.request = &request;
            return moneyscatter::advance(pool, request, particle, state, pose, services);
        }
        void updateAuxiliaryModel() override { ++services.auxiliaryUpdates; }
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
            moneyscatter::approachAngles(particle, {2000, -2000, -1}, true, pose);
            check(particle.angles == std::array<std::int16_t, 3>{1000, -1000, 0},
                "Ordinary angle steps retain the backward one-unit-short behavior.");
            particle.angles = {};
            moneyscatter::approachAngles(particle, {6000, -6000, -5000}, false, pose);
            check(particle.angles == std::array<std::int16_t, 3>{5000, -5000, -4999},
                "Suction multiplies the angle step by five without correcting the backward quirk.");
        }
        for (auto spin : {0.0f, 89.0f, 90.0f, 91.0f, 179.0f, 180.0f, 269.0f, 270.0f, 271.0f, 360.0f}) {
            moneyscatter::Particle particle;
            particle.motion = 2;
            particle.spin = spin;
            particle.rotationPhase = 32760;
            particle.angularVelocity = 10000;
            particle.forward = {0, 0, 1};
            moneyscatter::advanceOrientation(particle, true, pose);
            const bool rested = spin == 89 || spin == 90 || spin == 91 || spin == 269 || spin == 270 || spin == 271;
            check(particle.motion == (rested ? 3 : 2) && particle.rotationPhase == -22776 && particle.angularVelocity == 10300,
                "Settling must retain face thresholds, signed phase wrapping and angular acceleration.");
            check(particle.up == Vector{0, 0, 1}, "Settling restores the original orientation basis.");
        }
        {
            moneyscatter::Particle particle;
            particle.motion = 1;
            particle.velocity = {3, 4, 0};
            particle.forward = {0, 1, 1};
            particle.up = {2, 4, 0.9f};
            particle.spin = 350;
            moneyscatter::advanceOrientation(particle, true, pose);
            check(particle.spin == 12 && particle.up == Vector{1, 2, 1} && particle.forward[1] < particle.forward[2],
                "Rolling advances speed-dependent spin and relaxes orientation.");
            particle.motion = 3;
            particle.spin = 1000;
            moneyscatter::advanceOrientation(particle, true, pose);
            check(particle.spin == 640, "Spin wraps only once, not with a full modulo operation.");
            particle.motion = 2;
            particle.spin = 100;
            particle.angularVelocity = -10001;
            moneyscatter::advanceOrientation(particle, false, pose);
            check(particle.spin == 100 && particle.angularVelocity == -10001, "Suction bypasses settling progression.");
            moneyscatter::advanceOrientation(particle, true, pose);
            check(particle.angularVelocity == -10001, "Angular speed above the threshold is retained.");
        }
        for (int suction : {0, 1, 2, -1, 3}) {
            TestServices services;
            services.suctionResult = suction;
            moneyscatter::Pool pool({30, 5, 2});
            auto& request = pool.persistent();
            request.lifetime = 150;
            request.placedMoney = false;
            request.suctionReady = true;
            auto* particle = pool.particles.free.take();
            request.particles.append(*particle);
            particle->position = {50, 0, 0};
            particle->velocity = {10, 0, 0};
            particle->forward = {0, 0, 1};
            particle->lifetime = 20;
            particle->visible = true;
            particle->scale = 1;
            particle->sparkleCounter = 80;
            services.pool = &pool;
            services.request = &request;
            moneyscatter::UpdateState state;
            const bool alive = moneyscatter::advance(pool, request, *particle, state, pose, services);
            if (suction == 2) {
                check(!alive && services.calls == std::vector<unsigned>{1, 0x13} && pool.particles.free.count == 200,
                    "Suction capture exits before physics, effects and model updates.");
            } else {
                check(alive && particle->lifetime == (suction == 0 ? 19 : 20) &&
                    services.calls == std::vector<unsigned>{1, 2, 6, 7}, "Suction/lifetime/physics/sparkle/model order changed.");
                if (suction == 1) check(!state.ordinaryMotion && near(particle->scale, 0.6f) &&
                    near(services.queriedVelocity[0], 34.65f) && particle->rotationPhase == 0 && particle->angularVelocity == 0,
                    "Suction caps speed before physics, scales by distance and resets rotational state.");
            }
        }
        {
            TestServices services;
            Bindings bindings(pose, services);
            moneyscatter::Pool pool({30, 5, 2});
            auto* request = pool.scatter({0, 50, 0}, {0, 0, 1}, 2, 1, 4, 0, bindings);
            services.calls.clear();
            pool.update(bindings);
            check(request->particles.count == 0 && pool.particles.free.count == 200 && pool.active.count == 2 &&
                services.calls == std::vector<unsigned>{2, 2} && services.auxiliaryUpdates == 1,
                "Adjacent expired particles recycle safely through the real pool update without model updates.");
            pool.update(bindings);
            check(pool.active.count == 1 && services.auxiliaryUpdates == 2, "The empty request retires on the following update.");
        }
        {
            TestServices services;
            services.player = {0, 0, 0};
            services.contact.floor = 0;
            services.contact.floorNormal = {0, 1, 0};
            Bindings bindings(pose, services);
            moneyscatter::Pool pool({30, 5, 2});
            moneyscatter::Models models(pool, std::make_shared<j3dmodel::ModelData>(), 0);
            auto* request = pool.scatter({0, 50, 0}, {0, 0, 1}, 1, 100, 10, 0, bindings);
            request->collectionReady = true;
            auto* particle = request->particles.sentinel.next;
            particle->sparkleCounter = 80;
            services.calls.clear();
            pool.update(bindings);
            check(services.calls == std::vector<unsigned>{2, 0x26, 3, 6} && pool.particles.free.count == 200,
                "Contact pickup suppresses bounce audio, still emits due sparkle, then recycles without a model update.");
            check(particle->model && particle->model->matrix[7] == 50,
                "Collection retains the initialized J3D instance without an extra transform update after retirement.");
        }
        {
            TestServices services;
            services.visibleRoom = false;
            Bindings bindings(pose, services);
            moneyscatter::Pool pool({30, 5, 2});
            auto* request = pool.scatter({0, 50, 0}, {0, 0, 1}, 1, 100, 10, 0, bindings);
            request->suctionReady = true;
            auto* particle = request->particles.sentinel.next;
            particle->motion = 3;
            particle->sparkleCounter = 79;
            services.calls.clear();
            pool.update(bindings);
            check(particle->sparkleCounter == 79 && particle->lifetime == 99 && services.calls == std::vector<unsigned>{7},
                "Invisible particles skip suction and sparkle increment but keep lifetime/model updates.");
            particle->sparkleCounter = 80;
            services.calls.clear();
            pool.update(bindings);
            check(particle->sparkleCounter == 0 && services.calls == std::vector<unsigned>{6, 7},
                "Already-due sparkle still emits while invisible.");
        }
        {
            TestServices services;
            services.contact.floor = 0;
            services.contact.floorNormal = {0, 1, 0};
            Bindings bindings(pose, services);
            moneyscatter::Pool pool({30, 5, 2});
            moneyscatter::Models models(pool, std::make_shared<j3dmodel::ModelData>(), 0);
            auto* request = pool.scatter({0, 50, 0}, {0, 0, 1}, 1, 100, 10, 0, bindings);
            auto* particle = request->particles.sentinel.next;
            particle->sparkleCounter = 80;
            services.calls.clear();
            pool.update(bindings);
            check(services.calls == std::vector<unsigned>{2, 5, 6, 7},
                "An uncollected bounce plays sound before sparkle and model update.");
            check(particle->model->matrix[3] == particle->position[0] && particle->model->matrix[7] == particle->position[1] &&
                particle->model->matrix[11] == particle->position[2], "Connected particle updates write the owned J3D model transform.");
            services.calls.clear();
            pool.paused = true;
            const auto position = particle->position;
            const auto lifetime = particle->lifetime;
            pool.update(bindings);
            check(services.calls.empty() && services.auxiliaryUpdates == 1 && particle->position == position &&
                particle->lifetime == lifetime, "Pause blocks the connected particle and auxiliary-model updates.");
        }
        {
            TestServices services;
            moneyscatter::Pool pool({30, 5, 2});
            auto& request = pool.persistent();
            auto* particle = pool.particles.free.take();
            request.particles.append(*particle);
            particle->motion = 3;
            particle->lifetime = 1000;
            services.request = &request;
            moneyscatter::UpdateState state;
            check(moneyscatter::advance(pool, request, *particle, state, pose, services) && particle->lifetime == 1000,
                "Persistent request lifetime must prevent particle aging.");
            request.lifetime = 100;
            particle->lifetime = -32768;
            check(moneyscatter::advance(pool, request, *particle, state, pose, services) && particle->lifetime == 32767,
                "Ordinary particle aging preserves original signed-halfword wrapping.");
        }
        std::cout << "Money particle update checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}


