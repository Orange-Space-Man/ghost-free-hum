#include "../recovered/billscatterinitializer.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct Services : billscatter::InitializationServices {
        unsigned draws = 0, models = 0;
        billscatter::Pool* pool = nullptr;
        std::uint32_t random() override {
            return draws++ % 2 ? 0x80000000 : 0xffffffff;
        }
        std::uint32_t roomAt(const billscatter::Vector& position, std::uint32_t previous) override {
            check(previous == 0xffffffff && position == billscatter::Vector{11, 2, -2}, "Room lookup uses new spawn position.");
            check(draws == 2 * (models + 1), "Both random draws precede room lookup.");
            return 0x12000034;
        }
        void updateModel(billscatter::Particle& particle) override {
            check(particle.room == 0x12000034 && particle.currentRoom == particle.room,
                "Room state precedes model callback.");
            check(pool->requests.at(14).particles.count == models, "Model update precedes particle insertion.");
            ++models;
        }
    };
}

int main() {
    try {
        billscatter::Pool pool(std::make_shared<j3dmodel::ModelData>());
        Services services;
        services.pool = &pool;
        auto& retained = pool.particles.at(29);
        retained.visible = true;
        auto* model = retained.model.get();
        auto* request = billscatter::scatter(pool, {1, 2, 3}, {2, 0, -1}, 1.25, 8, 31, 0xffff, 3, services);
        check(request == &pool.requests.at(14) && request->particles.count == 30 && pool.particles.free.count == 0,
            "Admission takes tail request and permits partial particle allocation.");
        check(request->particles.sentinel.next == &retained && retained.model.get() == model && retained.visible,
            "Particle tail order and untouched model/visibility are retained.");
        check(request->count == 31 && request->delay == 8 && request->strength == 1.25f &&
            request->lifetime == -1 && !request->suctionReady && request->suctionCounter == 0,
            "Request fields preserve byte count and signed lifetime.");
        check(retained.velocity == billscatter::Vector{} && retained.acceleration == billscatter::Vector{} &&
            retained.forward == billscatter::Vector{2, 0, -1} && retained.up == billscatter::Vector{0, 0, 1} &&
            retained.angles == std::array<std::int16_t, 3>{0, 0x7fff, 0} && retained.scale == 0.75f &&
            retained.spin == 180 && retained.flutterCounter == 4 && retained.lifetime == -1,
            "Initialization preserves original basis, scale and rounded random endpoint.");
        check(services.draws == 60 && services.models == 30 && pool.active.count == 1,
            "Only allocated particles consume random/model services.");
        for (unsigned i = 0; i < 14; ++i) {
            auto* empty = billscatter::scatter(pool, {}, {}, 0, 0, 1, 10, 3, services);
            check(empty && empty->particles.count == 0, "Particle exhaustion still admits requests.");
        }
        check(!billscatter::scatter(pool, {}, {}, 0, 0, 1, 10, 3, services) && services.draws == 60,
            "Request exhaustion has no initialization side effects.");
        std::cout << "Bill initialization checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
