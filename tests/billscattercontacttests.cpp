#include "../recovered/billscattercontact.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    bool near(const billscatter::Vector& left, const billscatter::Vector& right) {
        for (unsigned i = 0; i < 3; ++i) {
            if (!std::isfinite(left[i]) || std::abs(left[i] - right[i]) > 0.0001f) return false;
        }
        return true;
    }

    struct Services : billscatter::ContactServices {
        mapcollision::SphereContact contact;
        billscatter::Vector position{}, velocity{};
        float radius = 0;
        unsigned calls = 0;

        mapcollision::SphereContact sweep(const billscatter::Vector& value, float size,
            const billscatter::Vector& displacement) override {
            ++calls;
            position = value;
            radius = size;
            velocity = displacement;
            return contact;
        }
    };
}

int main() {
    try {
        billscatter::Particle particle;
        Services services;
        particle.position = {1, 2, 3};
        particle.velocity = {3, -4, 0};
        particle.scale = 0.25f;
        services.contact.correction = {10, 20, 30};
        billscatter::collide(particle, 20, 2, services);
        check(services.calls == 1 && services.radius == 5 && near(services.position, {1, 2, 3}) &&
            near(services.velocity, {3, -4, 0}), "Sweep receives scaled radius and retained motion.");
        check(near(particle.position, {1, 2, 3}) && near(particle.velocity, {3, -4, 0}),
            "No-contact correction must be ignored.");

        services.contact.floor = 0;
        services.contact.floorNormal = {0, 1, 0};
        billscatter::collide(particle, 20, 2, services);
        check(near(particle.position, {11, 22, 33}) && near(particle.velocity, {3, 0, 0}) && particle.motion == 0,
            "Floor projection removes normal motion without preserving incoming speed.");

        particle.velocity = {0, -2, 0};
        billscatter::collide(particle, 20, 2, services);
        check(particle.motion == 1 && near(particle.velocity, {0, -2, 0}),
            "Threshold equality settles without clearing velocity.");
        particle.velocity = {};
        services.contact.wall = 0;
        services.contact.wallNormal = {1, 0, 0};
        billscatter::collide(particle, 20, 2, services);
        check(near(particle.velocity, {}), "Zero-speed contact avoids normalization.");

        particle.motion = 0;
        particle.velocity = {3, -4, 2};
        services.contact.wallNormal = {0.6f, 0.8f, 0};
        billscatter::collide(particle, 20, 2, services);
        check(near(particle.velocity, {1.92f, -1.44f, 2}), "Wall response must use floor-projected velocity.");

        services.contact.floor = -1;
        services.contact.wallNormal = {2, 0, 0};
        particle.velocity = {3, 4, 0};
        billscatter::collide(particle, 20, 2, services);
        check(near(particle.velocity, {0, 16, 0}) && particle.motion == 0,
            "Original double cross preserves non-unit-normal scaling; walls do not settle.");
        std::cout << "Bill collision checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
