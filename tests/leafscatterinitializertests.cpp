#include "../recovered/leafscatterowner.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Services : leafscatter::InitializationServices {
        unsigned draws = 0, sounds = 0, counts = 0;
        std::uintptr_t handle = 7;
        std::uint8_t lastCount = 0;
        std::uint32_t random() override { return draws++ % 3 == 0 ? 0xffffffff : 0; }
        std::uint32_t roomAt(const billscatter::Vector& position, std::uint32_t previous) override {
            check(position == billscatter::Vector{6, 2, 3} && previous == 0xffffffff && draws % 3 == 0,
                "Leaf room lookup follows three draws at spawn position.");
            return 0x01000002;
        }
        std::uintptr_t startSound(unsigned id, const billscatter::Vector& position) override {
            check(id == 0x186e && position == billscatter::Vector{6, 2, 3}, "Leaf sound uses offset position.");
            ++sounds;
            return handle;
        }
        void soundCount(std::uintptr_t value, std::uint8_t count) override {
            check(value == handle && value != 0, "Sound count requires returned handle.");
            ++counts;
            lastCount = count;
        }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required.");
        std::ifstream input(argv[1], std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        actormodel::InitialPose pose(bytes);
        leafscatter::Owner owner;
        Services services;
        auto spawn = [&](std::uint8_t count) { return owner.scatter({1, 2, 3}, {1, 0, 0}, 2, 4, count, 200, pose, services); };
        check(!spawn(1) && services.sounds == 0 && services.draws == 0, "Inactive spawning has no side effects.");
        owner.initialize(std::make_shared<j3dmodel::ModelData>());
        auto* request = spawn(51);
        check(request == &owner.storage()->requests.at(14) && request->particles.count == 50 && services.lastCount == 51,
            "Tail allocation preserves requested audio count despite partial allocation.");
        const auto& particle = owner.storage()->particles.at(49);
        check(request->particles.sentinel.next == &particle && particle.sizeRatio == 135 && particle.scale == (0.75f * 135) / 255,
            "Original size formula preserves rounded endpoint and tail order.");
        check(particle.forward == billscatter::Vector{0, -1, 0} && particle.lifetime == 200 && particle.room == 0x01000002 &&
            particle.model->matrix[3] == 6 && particle.model->matrix[7] == 2 && particle.model->scale[0] == particle.scale,
            "Leaf initialization writes distinct direction, room and real owned model.");
        check(services.draws == 150 && services.sounds == 1 && services.counts == 1, "Three random draws per allocated leaf.");
        services.handle = 0;
        for (unsigned i = 0; i < 14; ++i) check(spawn(0), "Zero-count request is admitted.");
        check(services.sounds == 15 && services.counts == 1 && services.draws == 150,
            "Zero-count requests still start sound but null handles skip count updates.");
        check(!spawn(1) && services.sounds == 15, "Exhausted requests do not start sound.");
        std::cout << "Leaf initialization checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
