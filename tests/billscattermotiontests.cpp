#include "../recovered/billscattermotion.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
    struct Services : billscatter::ContactServices {
        unsigned calls = 0;
        billscatter::Vector position{}, velocity{};
        mapcollision::SphereContact sweep(const billscatter::Vector& p, float radius, const billscatter::Vector& v) override {
            ++calls;
            position = p;
            velocity = v;
            check(radius > 0, "Scaled collision radius is supplied.");
            return {};
        }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required.");
        std::ifstream input(argv[1], std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        actormodel::InitialPose pose(bytes);
        std::uint32_t particleRandom = 7, gameplayRandom = 11;
        auto expected = particleRandom * 0x5eedf715u + 0x1b0cb173u;
        const auto first = pose.horizontalDirection(static_cast<std::uint16_t>((expected % 2048) * 16));
        expected = expected * 0x5eedf715u + 0x1b0cb173u;
        const auto second = pose.horizontalDirection(static_cast<std::uint16_t>((expected % 2048) * 16));
        const auto direction = billscatter::flutterDirection(particleRandom, pose);
        check(particleRandom == expected && direction == billscatter::Vector{first[0] * second[2], first[0] * second[0], first[2]},
            "Flutter uses two particle RNG draws and original table indexes.");
        Services services;
        billscatter::Request request;
        billscatter::Particle particle;
        particle.scale = 0.5f;
        particle.acceleration = {20, 0, 0};
        billscatter::move(particle, request, true, 1, 3, particleRandom, gameplayRandom, pose, services);
        check(near(particle.velocity[0], 19) && near(particle.velocity[1], -1.5f) && near(particle.acceleration[0], 9),
            "Force is copied before acceleration limiting; gravity affects only force.");
        check(particleRandom == expected && gameplayRandom == 11 && particle.flutterCounter == 1,
            "Counter does not draw random values before its strict threshold.");
        check(near(particle.scale, 0.575f) && services.position == particle.position && services.velocity == particle.velocity,
            "Scale recovery and movement precede collision.");
        particle.flutterCounter = 4;
        billscatter::move(particle, request, true, 1, 3, particleRandom, gameplayRandom, pose, services);
        expected = (expected * 0x5eedf715u + 0x1b0cb173u) * 0x5eedf715u + 0x1b0cb173u;
        check(particleRandom == expected && gameplayRandom == 11u * 0x19660du + 0x3c6ef35fu && particle.flutterCounter == 0,
            "Flutter advances distinct generators and resets counter.");
        particle.motion = 1;
        billscatter::move(particle, request, true, 1, 3, particleRandom, gameplayRandom, pose, services);
        check(services.calls == 2, "Settled motion skips movement and collision.");
        particle.motion = 0;
        particle.velocity = {};
        particle.acceleration = {100, 0, 0};
        billscatter::move(particle, request, false, 1, 3, particleRandom, gameplayRandom, pose, services);
        check(near(particle.velocity[0], 33) && particle.acceleration[0] == 100 && particle.flutterCounter == 0,
            "Attracted motion skips gravity, flutter and acceleration damping but caps velocity.");
        std::cout << "Bill flutter checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
