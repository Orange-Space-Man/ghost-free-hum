#include "game/effects/particlebatch.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Archive : particles::ArchiveResources {
        std::vector<std::string> names;
        particles::Resource get(std::uint32_t type, std::string_view name) override {
            check(type == 0x4b485f45, "Money effects use the original particle archive route.");
            names.emplace_back(name);
            return std::make_shared<std::vector<std::uint8_t>>(0xc0);
        }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the local executable for original particle resource tables.");
        std::ifstream stream(argv[1], std::ios::binary);
        check(bool(stream), "Cannot open original executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        {
            Archive archive;
            particles::EmitterPool pool(2, 0, 0);
            particles::Batch batch(executable, archive, pool, 2, 0);
            check(archive.names == std::vector<std::string>{"coin_ef.pcr", "star5_03.bti", "ps_6kaku.bti"},
                "Coin sparkle batch must resolve its original resource table entries.");
            const auto attempts = pool.attempts;
            batch.emit({1, 2, 3});
            batch.emit({4, 5, 6});
            batch.emit({7, 8, 9});
            check(batch.cursor == 2 && pool.attempts == attempts && batch.entries()[0]->position == std::array<float, 3>{1, 2, 3} &&
                batch.entries()[1]->position == std::array<float, 3>{4, 5, 6} && batch.entries()[0]->flags == 0xe,
                "Sparkles reuse slots, enable emission, and ignore requests beyond batch capacity.");
            batch.update();
            check(batch.cursor == 0 && batch.entries()[0]->flags == 0xc && batch.entries()[1]->flags == 0xc,
                "Frame reset disables emission and rewinds the original cursor.");
            batch.emit({7, 8, 9});
            check(batch.cursor == 1 && batch.entries()[0]->position == std::array<float, 3>{7, 8, 9}, "Next frame reuses the first slot.");
        }
        {
            Archive archive;
            particles::EmitterPool pool(1, 0, 0);
            auto resource = std::make_shared<std::vector<std::uint8_t>>(0xc0);
            auto* blocker = pool.create(resource, resource, resource);
            particles::Batch batch(executable, archive, pool, 1, 0);
            check(!batch.entries()[0], "Exhausted pool must leave a failed batch uninitialized.");
            pool.destination->remove(*blocker);
            pool.emitters.free.append(*blocker);
            batch.emit({1, 2, 3});
            check(batch.entries()[0] && batch.cursor == 0 && batch.entries()[0]->flags == 0xc &&
                batch.entries()[0]->position == std::array<float, 3>{}, "Successful allocation retry must not also emit on that call.");
            batch.emit({1, 2, 3});
            check(batch.cursor == 1 && batch.entries()[0]->flags == 0xe, "Emission starts on the call after allocation recovers.");
        }
        {
            Archive archive;
            particles::EmitterPool pool(1, 0, 0);
            particles::EffectFactory factory(executable, archive, pool);
            factory.history[0] = 99;
            auto* emitter = factory.createCoinCollection(0, {10, 20, 30});
            check(emitter && emitter->position == std::array<float, 3>{10, 20, 30} && emitter->field220 == std::array<float, 3>{} &&
                emitter->field22C == std::array<float, 3>{} && emitter->fieldC4 && !emitter->fieldC8 &&
                factory.history[0] == 310 && factory.history[1] == 99, "Collection allocation initializes vectors, callback and effect history.");
            for (auto age : {0u, 1u, 2u, 65535u}) {
                emitter->fieldBC = static_cast<std::uint16_t>(age);
                emitter->flags = 0xa2;
                emitter->fieldC4->advance(*emitter);
                check(emitter->flags == (age > 1 ? 0x1c : 0xa2), "Collection callback replaces flags only after age one.");
            }
            check(!factory.createCoinCollection(1, {}) && factory.history[0] == 311 && factory.history[1] == 310 && archive.names.size() == 6,
                "Failed collection allocation still shifts history and resolves resources.");
        }
        std::cout << "Money effect routing checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
