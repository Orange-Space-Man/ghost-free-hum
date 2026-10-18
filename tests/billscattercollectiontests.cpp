#include "../recovered/billscattercollection.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Services : billscatter::CollectionServices {
        billscatter::Particle* particle = nullptr;
        billscatter::Request* request = nullptr;
        billscatter::Pool* pool = nullptr;
        unsigned events = 0;
        billscatter::Vector playerPosition() override { return {0, 10, 0}; }
        float playerTop() override { return 30; }
        void playerEvent(unsigned event, const playerevents::Payload& payload) override {
            ++events;
            if (event == 0x27) {
                check(payload.word(0x98) == 1 && particle->lifetime == 20,
                    "Contact dispatch sets bill count before expiring particle.");
            } else {
                check(event == 0x13 && payload.word(0x60) == 6 && payload.word(0x64) == 1,
                    "Suction dispatch carries bill type and count.");
                check(request->particles.count == 0 && pool->particles.free.sentinel.previous == particle,
                    "Suction recycles before dispatch.");
                check(particle->lifetime == 20, "Suction recycling retains particle fields.");
            }
        }
    };
}

int main() {
    try {
        billscatter::Pool pool(std::make_shared<j3dmodel::ModelData>());
        auto* particle = pool.particles.free.take();
        auto* request = pool.requests.free.take();
        request->particles.append(*particle);
        Services services;
        services.pool = &pool;
        services.request = request;
        services.particle = particle;
        particle->lifetime = 20;
        particle->position = {60, 10, 0};
        check(billscatter::collect(*particle, services) && particle->lifetime == 0 && request->particles.count == 1,
            "Inclusive lower/radius boundary expires without unlinking.");
        particle->lifetime = 20;
        particle->position = {0, 30, 60};
        check(billscatter::collect(*particle, services), "Upper height boundary is inclusive.");
        particle->lifetime = 20;
        for (const auto position : {billscatter::Vector{60.01f, 20, 0}, {0, 9, 0}, {0, 31, 0}}) {
            particle->position = position;
            check(!billscatter::collect(*particle, services) && particle->lifetime == 20,
                "Out-of-range collection leaves lifetime unchanged.");
        }
        particle->position = {};
        billscatter::scaleSuction(*particle, {});
        check(std::abs(particle->scale - 0.075f) < 0.000001f, "Coincident suction retains minimum scale.");
        particle->position = {0, 75, 0};
        billscatter::scaleSuction(*particle, {});
        check(std::abs(particle->scale - 0.4125f) < 0.000001f, "Suction scale uses full three-dimensional distance.");
        particle->position = {151, 0, 0};
        billscatter::scaleSuction(*particle, {});
        check(particle->scale == 0.75f, "Beyond-range scale is capped.");
        billscatter::collectSuction(pool, *request, *particle, services);
        check(services.events == 3, "Rejected contact does not dispatch events.");
        std::cout << "Bill collection checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
