#include "../recovered/leafscatterowner.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Services : billscatter::Services {
        unsigned calls = 0;
        std::uint32_t roomAt(const billscatter::Vector&, std::uint32_t) override { ++calls; return 0; }
        std::uint16_t visibilityFlags(std::uint32_t) override { return 1; }
        bool advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle) override {
            pool.recycle(request, particle);
            return false;
        }
    };
}

int main() {
    try {
        leafscatter::Owner owner;
        Services services;
        owner.update(false, 30, 2, services);
        owner.release();
        check(!owner.active() && !owner.storage() && services.calls == 0, "Inactive owner does no update work.");
        auto model = std::make_shared<j3dmodel::ModelData>();
        std::weak_ptr<j3dmodel::ModelData> retained = model;
        owner.initialize(model);
        auto* storage = owner.storage();
        check(owner.active() && storage->particles.free.count == 50 && storage->requests.free.count == 15 &&
            storage->active.count == 0 && owner.color() == std::array<std::uint16_t, 3>{255, 255, 255},
            "Leaf initialization creates original capacity and color state without reserving a request.");
        for (unsigned i = 0; i < 50; ++i) {
            check(bool(storage->particles.at(i).model), "Each leaf owns a model.");
            if (i) check(storage->particles.at(i).model != storage->particles.at(i - 1).model, "Model instances are distinct.");
        }
        auto* request = storage->requests.free.take();
        auto* particle = storage->particles.free.take();
        request->lifetime = 10;
        request->particles.append(*particle);
        storage->active.append(*request);
        owner.initialize(std::make_shared<j3dmodel::ModelData>());
        check(owner.storage() == storage && storage->active.count == 1, "Repeated initialization preserves active storage.");
        owner.update(true, 30, 2, services);
        check(request->lifetime == 10 && services.calls == 0, "Global pause suppresses active leaf update.");
        owner.update(false, 30, 2, services);
        check(services.calls == 1 && storage->particles.free.count == 50 && request->lifetime == 9,
            "Active gate delegates the shared request traversal to the leaf callback.");
        owner.update(false, 30, 2, services);
        check(storage->active.count == 0 && storage->requests.free.count == 15, "Empty request retires on following frame.");
        model.reset();
        owner.release();
        check(retained.expired() && !owner.active(), "Release drops all retained model resources.");
        owner.release();
        owner.initialize(std::make_shared<j3dmodel::ModelData>());
        check(owner.storage()->particles.free.count == 50 && owner.storage()->active.count == 0,
            "Reactivation creates fresh pool storage.");
        std::cout << "Leaf owner checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
