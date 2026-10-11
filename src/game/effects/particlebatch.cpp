#include "resources/dolresources.h"
#include "game/effects/particlebatch.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    std::string name(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto pointer = range(executable, address, 4);
        address = (unsigned(pointer[0]) << 24) | (unsigned(pointer[1]) << 16) | (unsigned(pointer[2]) << 8) | pointer[3];
        std::string result;
        for (unsigned i = 0; i < 256; ++i) {
            const auto value = range(executable, address + i, 1)[0];
            if (!value) return result;
            result += static_cast<char>(value);
        }
        throw std::runtime_error("Original particle batch resource name is unterminated.");
    }
}

particles::Batch::Batch(const std::vector<std::uint8_t>& executable, ArchiveResources& resources, EmitterPool& pool,
    unsigned count, unsigned type) : executable(executable), resources(resources), pool(pool), type(type),
    initialized(false), slots(count, nullptr) {
    if (type > 3) throw std::out_of_range("Original particle batch type is invalid.");
    initialized = allocate();
}

bool particles::Batch::allocate() {
    const auto parameters = resources.get(0x4b485f45, name(executable, 0x80383b1c + type * 4));
    const auto texture = resources.get(0x4b485f45, name(executable, 0x80383b58 + type * 4));
    const auto alternate = resources.get(0x4b485f45, name(executable, 0x80383b78 + type * 4));
    for (auto& slot : slots) {
        slot = pool.create(parameters, texture, alternate);
        if (!slot) return false;
        slot->position = slot->field220 = slot->field22C = {};
        slot->fieldC4 = nullptr;
        slot->fieldC8 = nullptr;
        slot->flags = 0xc;
    }
    return true;
}

void particles::Batch::update() {
    cursor = 0;
    if (!initialized) initialized = allocate();
    else for (auto* slot : slots) if (slot) slot->flags = 0xc;
}

const std::vector<particles::Emitter*>& particles::Batch::entries() const { return slots; }
