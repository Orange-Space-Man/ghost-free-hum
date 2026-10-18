#include "../recovered/billscatterpool.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Services : billscatter::Services {
        std::vector<billscatter::Pool*> visited;
        std::vector<unsigned> calls;
        bool expire = false;
        std::uint32_t roomAt(const billscatter::Vector&, std::uint32_t previous) override {
            check(previous == 0xffffffff, "Bill room lookup preserves retained previous-room input.");
            calls.push_back(1);
            return 7;
        }
        std::uint16_t visibilityFlags(std::uint32_t room) override {
            check(room == 7, "Visibility must use the resolved room.");
            calls.push_back(2);
            return 1;
        }
        bool advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle) override {
            visited.push_back(&pool);
            calls.push_back(3);
            if (expire) pool.recycle(request, particle);
            return !expire;
        }
    };
}

int main() {
    try {
        std::array<std::shared_ptr<j3dmodel::ModelData>, 3> models;
        for (auto& model : models) model = std::make_shared<j3dmodel::ModelData>();
        billscatter::Owners owners(models);
        Services services;
        std::array<billscatter::Request*, 3> requests;
        std::array<billscatter::Particle*, 3> particles;
        for (unsigned i = 0; i < 3; ++i) {
            auto& pool = *owners.pools[i];
            check(pool.requests.free.count == 15 && pool.particles.free.count == 30 && pool.active.count == 0,
                "Each bill owner requires 15 requests and 30 particles, with no persistent reservation.");
            for (unsigned j = 0; j < 30; ++j) {
                check(pool.particles.at(j).model->model == models[i], "Bill variants must retain their separate shared model resources.");
                if (j) check(pool.particles.at(j).model.get() != pool.particles.at(j - 1).model.get(), "Each bill retains a distinct J3D instance.");
            }
            requests[i] = pool.requests.free.take();
            particles[i] = pool.particles.free.take();
            pool.active.append(*requests[i]);
            requests[i]->particles.append(*particles[i]);
            requests[i]->lifetime = i == 0 ? -32768 : 1000;
            requests[i]->delay = 2;
            particles[i]->lifetime = 50;
        }
        owners.update(false, 1, 2, services);
        check(services.visited == std::vector<billscatter::Pool*>{owners.pools[0].get(), owners.pools[1].get(), owners.pools[2].get()},
            "Bill owners advance in original model order.");
        check(services.calls == std::vector<unsigned>{1, 2, 3, 1, 2, 3, 1, 2, 3}, "Room, visibility and particle progression order changed.");
        check(requests[0]->lifetime == 32767 && requests[1]->lifetime == 999 && requests[0]->delay == 1 && !requests[0]->suctionReady,
            "All bill requests age, including 1000, with signed wrapping and decrementing delay.");
        check(particles[0]->blinking && particles[0]->visible && particles[0]->blinkCounter == 0,
            "Blink activation occurs after evaluating existing blink state.");
        owners.update(false, 1, 2, services);
        check(requests[0]->suctionReady && requests[0]->delay == 0 && particles[0]->blinkCounter == 1,
            "Suction activates after reaching its counter threshold and delay stops at zero.");
        owners.update(false, 1, 2, services);
        owners.update(false, 1, 2, services);
        check(!particles[0]->visible && particles[0]->blinkCounter == 1, "Blink threshold suppresses visibility before particle advance.");
        services.calls.clear();
        const auto lifetime = requests[0]->lifetime;
        owners.update(true, 1, 2, services);
        check(services.calls.empty() && requests[0]->lifetime == lifetime, "Shared pause freezes all bill owners.");
        auto& first = *owners.pools[0];
        auto* second = first.particles.free.take();
        requests[0]->particles.append(*second);
        services.expire = true;
        services.visited.clear();
        owners.update(false, 1, 2, services);
        check(services.visited.size() == 4 && first.particles.free.count == 30 && first.active.count == 1,
            "Adjacent expired particles recycle safely while requests survive until the following update.");
        auto* retainedModel = particles[0]->model.get();
        owners.update(false, 1, 2, services);
        for (const auto& pool : owners.pools) check(pool->active.count == 0 && pool->requests.free.count == 15,
            "Empty bill requests return to the free pool without a permanent request.");
        check(particles[0]->model.get() == retainedModel, "Bill recycling retains J3D ownership.");
        std::cout << "Bill pool ownership and update checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
