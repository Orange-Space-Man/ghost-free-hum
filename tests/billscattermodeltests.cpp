#include "../recovered/billscattermodels.h"
#include "../recovered/billscatterinitializer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    bool near(float a, float b) { return std::isfinite(a) && std::abs(a - b) < 0.0001f; }
    struct Services : billscatter::InitializationServices {
        std::vector<std::uint32_t> values;
        unsigned draws = 0;
        std::vector<j3dmodel::ModelInstance*> models;
        std::uint32_t random() override { return draws < values.size() ? values[draws++] : (++draws, 0); }
        std::uint32_t roomAt(const billscatter::Vector&, std::uint32_t) override { return 0; }
        void updateModel(billscatter::Particle& particle) override {
            billscatter::updateModel(particle);
            models.push_back(particle.model.get());
        }
    };
}

int main() {
    try {
        std::array<std::shared_ptr<j3dmodel::ModelData>, 3> resources;
        for (auto& resource : resources) resource = std::make_shared<j3dmodel::ModelData>();
        billscatter::Owners owners(resources);
        auto& particle = owners.pools[0]->particles.at(0);
        particle.forward = {2, 0, 0};
        particle.up = {1, 0, 3};
        particle.position = {7, 8, 9};
        particle.scale = 0.75f;
        particle.spin = 90;
        billscatter::updateModel(particle);
        const auto& matrix = particle.model->matrix;
        check(near(matrix[0], 2) && near(matrix[6], -1) && near(matrix[9], 1),
            "Bill basis uses forward as X and spins around local X.");
        check(matrix[3] == 7 && matrix[7] == 8 && matrix[11] == 9 && particle.model->scale[0] == 0.75f,
            "Translation and uniform scale remain separate.");
        check(particle.up == billscatter::Vector{0, 0, 1}, "Up vector is repaired and retained.");
        particle.forward = {};
        billscatter::updateModel(particle);
        check(matrix[0] == 0 && matrix[5] == 0 && matrix[10] == 0, "Degenerate basis is not replaced.");

        Services services;
        services.values = {0x80000000, 0, 0, 0xffffffff};
        billscatter::scatter(owners, {1, 2, 3}, {1, 0, 0}, 2, 5, 5, 100, 3, services);
        check(services.models.size() == 4 && services.draws == 10,
            "Remainders consume interleaved random draws; rounded variant three is dropped.");
        check(services.models[0] == owners.pools[1]->particles.at(29).model.get() &&
            services.models[1] == owners.pools[0]->particles.at(29).model.get() &&
            services.models[2] == owners.pools[1]->particles.at(28).model.get() &&
            services.models[3] == owners.pools[2]->particles.at(29).model.get(),
            "Remainders precede equal portions in original owner order.");
        check(services.models[0]->matrix[3] == 6 && services.models[0]->scale[0] == 0.75f,
            "Admission writes an actual owned model transform.");
        billscatter::scatter(owners, {}, {}, 0, 0, 0, 0, 3, services);
        check(services.draws == 10, "Zero count consumes no random values.");
        std::cout << "Bill model and distribution checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
