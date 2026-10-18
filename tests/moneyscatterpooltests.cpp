#include "../recovered/moneyscatterpool.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct Services final : moneyscatter::Services {
        std::vector<moneyscatter::Particle*> initialized, visited;
        std::vector<unsigned> calls;
        bool expire = false;
        unsigned auxiliaryUpdates = 0;
        void initializeParticle(moneyscatter::Request& request, moneyscatter::Particle& particle) override {
            initialized.push_back(&particle);
            particle.position = request.position;
            particle.lifetime = request.lifetime;
            particle.blinking = false;
            particle.blinkCounter = 0;
        }
        std::uint32_t roomAt(const moneyscatter::Vector&, std::uint32_t previous) override {
            check(previous == 0xffffffff, "Room queries retain the original room seed.");
            calls.push_back(1);
            return 7;
        }
        std::uint16_t roomFlags(std::uint32_t room) override {
            check(room == 7, "Visibility queries use the resolved room.");
            calls.push_back(2);
            return 1;
        }
        bool advanceParticle(moneyscatter::Pool& pool, moneyscatter::Request& request, moneyscatter::Particle& particle) override {
            calls.push_back(3);
            visited.push_back(&particle);
            if (expire) pool.recycle(request, particle);
            return !expire;
        }
        void updateAuxiliaryModel() override { ++auxiliaryUpdates; calls.push_back(4); }
    };
}

int main() {
    try {
        const moneyscatter::Timing timing{30, 5, 2};
        const moneyscatter::Vector position{10, 20, 30}, direction{1, 2, 3};
        {
            Services services;
            moneyscatter::Pool pool(timing);
            check(pool.requests.free.count == 34 && pool.particles.free.count == 200 && pool.active.count == 1,
                "The original pool reserves one of its 35 requests and retains all 200 particles.");
            check(&pool.persistent() == &pool.requests.at(34) && pool.persistent().lifetime == 1000 && pool.persistent().placedMoney,
                "The last request is reserved for persistent placed money.");
            auto* request = pool.scatter(position, direction, 255, 150, 30.125, 0, services);
            check(request == &pool.requests.at(33) && request->particles.count == 200 && !pool.particles.free.count,
                "Exhausted particle allocation preserves a partial request rather than growing the pool.");
            check(request->count == 255 && request->speed == 30.125f && request->direction == direction && request->position == position,
                "Admission retains the requested count and original input fields.");
            check(services.initialized.front() == &pool.particles.at(199) && services.initialized.back() == &pool.particles.at(0),
                "Particle acquisition starts at the original free-list tail.");
            auto* empty = pool.scatter(position, direction, 1, 150, 30, 0, services);
            check(empty && !empty->particles.count, "A particle-empty request is still admitted when a request slot exists.");
            services.expire = true;
            pool.update(services);
            check(services.visited == services.initialized && pool.particles.free.count == 200,
                "Particle release does not skip neighbors or follow recycled free-list links.");
            check(pool.active.count == 2 && pool.requests.free.count == 33,
                "An initially empty request recycles now; a request emptied during its update recycles on the next update.");
            pool.update(services);
            check(pool.active.count == 1 && pool.requests.free.count == 34,
                "The permanent empty request survives while ordinary empty requests return to the pool.");
            services.expire = false;
            auto* reused = pool.scatter(position, direction, 1, 150, 30, 0, services);
            check(reused == request && services.initialized.back() == &pool.particles.at(0),
                "Recycled request and particle slots preserve original LIFO reuse.");
        }
        {
            Services services;
            moneyscatter::Pool pool(timing);
            for (unsigned index = 0; index < 34; ++index)
                check(pool.scatter(position, direction, 0, 150, 30, 0, services) != nullptr, "Request capacity shrank.");
            check(!pool.scatter(position, direction, 1, 150, 30, 0, services) && services.initialized.empty(),
                "Request exhaustion returns failure without consuming a particle.");
            pool.update(services);
            check(pool.requests.free.count == 34 && pool.active.count == 1,
                "All adjacent empty requests recycle in one traversal.");
        }
        {
            Services services;
            moneyscatter::Pool pool(timing);
            auto* request = pool.scatter(position, direction, 1, 150, 30, 0, services);
            auto& particle = *request->particles.sentinel.next;
            pool.paused = true;
            pool.update(services);
            check(request->lifetime == 150 && !request->suctionCounter && services.calls.empty(),
                "Pause freezes timers, particle services and the auxiliary model.");
            pool.paused = false;
            for (unsigned frame = 0; frame < 5; ++frame) pool.update(services);
            check(!request->collectionReady && request->collectionCounter == 5, "Collection delay does not activate early.");
            pool.update(services);
            check(request->collectionReady, "Collection activates on the update after reaching its threshold.");
            for (unsigned frame = 6; frame < 30; ++frame) pool.update(services);
            check(!request->suctionReady && request->suctionCounter == 30, "Suction delay preserves its boundary.");
            pool.update(services);
            check(request->suctionReady && request->lifetime == 119 && pool.persistent().lifetime == 1000,
                "Suction activates after 30 increments; permanent lifetimes do not decrement.");
            particle.lifetime = 50;
            pool.update(services);
            check(particle.blinking && particle.visible && !particle.blinkCounter,
                "Blinking begins after the visibility phase at lifetime 50.");
            pool.update(services);
            pool.update(services);
            check(particle.visible && particle.blinkCounter == 2, "Blink threshold is tested before increment.");
            pool.update(services);
            check(!particle.visible && particle.blinkCounter == 1, "The blink hides one update and resets before incrementing.");
            pool.update(services);
            check(particle.visible, "Room visibility restores the following blink frame.");
            services.calls.clear();
            pool.update(services);
            check(services.calls == std::vector<unsigned>{1, 2, 3, 4},
                "Room lookup, visibility, particle update and auxiliary model retain their order.");
            request->lifetime = -32768;
            pool.update(services);
            check(request->lifetime == 32767, "Signed lifetime decrement preserves original 16-bit wrap.");
        }
        std::cout << "Money scatter pool checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
