#include "../recovered/billscatterupdate.h"
#include "../recovered/billscattermodels.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Services : billscatter::UpdateServices {
        int result = 0;
        unsigned models = 0, events = 0, suctions = 0;
        bool touching = false;
        billscatter::Request* request = nullptr;
        int suction(billscatter::Particle&) override { ++suctions; return result; }
        billscatter::Vector suctionPosition() override { return {}; }
        billscatter::Vector playerPosition() override { return touching ? billscatter::Vector{} : billscatter::Vector{10000, 0, 0}; }
        float playerTop() override { return 100; }
        void playerEvent(unsigned event, const playerevents::Payload& payload) override {
            ++events;
            if (event == 0x13) check(request->particles.count == 0 && payload.word(0x60) == 6, "Suction event follows recycling.");
            else check(event == 0x27 && request->particles.count == 1, "Contact event precedes recycling.");
        }
        mapcollision::SphereContact sweep(const billscatter::Vector&, float, const billscatter::Vector&) override { return {}; }
        void updateModel(billscatter::Particle& particle) override { ++models; billscatter::updateModel(particle); }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required.");
        std::ifstream input(argv[1], std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        actormodel::InitialPose pose(bytes);
        billscatter::Pool pool(std::make_shared<j3dmodel::ModelData>());
        auto* request = pool.requests.free.take();
        auto* particle = pool.particles.free.take();
        request->particles.append(*particle);
        particle->forward = {0, 0, 1};
        particle->up = {0, 1, 0};
        particle->motion = 1;
        particle->spin = 90;
        particle->velocity = {2, 3, 4};
        particle->acceleration = {1, 2, 3};
        billscatter::advanceOrientation(*particle, true, pose);
        check(particle->spin == 90 && particle->velocity == billscatter::Vector{2, 0, 4} && particle->acceleration == billscatter::Vector{},
            "Settling preserves exact quadrant and horizontal velocity.");
        for (const auto pair : {std::pair{80.0f, 65.0f}, {100.0f, 115.0f}, {190.0f, 180.0f}, {350.0f, 0.0f}, {270.0f, 270.0f}}) {
            particle->spin = pair.first;
            billscatter::advanceOrientation(*particle, true, pose);
            check(particle->spin == pair.second, "Original settling ranges and inclusive wrap are retained.");
        }
        Services services;
        services.request = request;
        std::uint32_t particleRandom = 1, gameplayRandom = 2;
        auto advance = [&] { return billscatter::advance(pool, *request, *particle, 1, 3, particleRandom, gameplayRandom, pose, services); };
        particle->lifetime = 1000;
        check(advance() && particle->lifetime == 999 && services.models == 1 && services.suctions == 0,
            "Inactive suction ages bills including lifetime 1000 and updates owned models.");
        request->suctionReady = particle->visible = true;
        services.result = -1;
        check(advance() && particle->lifetime == 999, "Unexpected suction result preserves original no-aging branch.");
        services.result = 1;
        particle->velocity = particle->acceleration = {};
        check(advance() && particle->motion == 0 && particle->lifetime == 999,
            "Attraction reactivates motion without aging.");
        services.result = 2;
        const auto models = services.models;
        check(!advance() && services.events == 1 && services.models == models && request->particles.count == 0,
            "Captured bill returns before movement and model update.");
        pool.particles.free.remove(*particle);
        request->particles.append(*particle);
        particle->motion = 1;
        particle->position = {};
        particle->lifetime = 2;
        services.result = 0;
        services.touching = true;
        check(!advance() && services.events == 2 && services.models == models && request->particles.count == 0,
            "Contact collection expires and recycles before model update.");
        std::cout << "Bill update checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
