#include "resources/dolresources.h"
#include "game/effects/particleeffectfactory.h"
#include "game/startup/startupresources.h"
#include "game/effects/particlesuction.h"

#include <cstring>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::string name(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::string result;
        for (unsigned i = 0; i < 256; ++i) {
            const auto value = range(executable, address + i, 1).front();
            if (!value) return result;
            result += static_cast<char>(value);
        }
        throw std::runtime_error("Original particle resource name is unterminated.");
    }
}

particles::EffectFactory::EffectFactory(const std::vector<std::uint8_t>& executable, ArchiveResources& resources,
    EmitterPool& pool) : executable(executable), resources(resources), pool(pool) {}

void particles::EffectFactory::createExplosion(unsigned type, const std::array<float, 3>& position) {
    if (type >= 4) throw std::out_of_range("Original explosion type is invalid.");
    for (unsigned i = 49; i > 0; --i) history[i] = history[i - 1];
    history[0] = type + 250;
    std::array<Resource, 8> assets;
    const auto table = range(executable, 0x8020d15c, 128);
    for (unsigned i = 0; i < 4; ++i) {
        assets[i * 2] = resources.get(0x4b485f45, name(executable, word(table, i * 16 + type * 4)));
        assets[i * 2 + 1] = resources.get(0x4b485f45, name(executable, word(table, 64 + i * 16 + type * 4)));
    }
    for (unsigned i = 0; i < 4; ++i) {
        auto* emitter = pool.create(assets[i * 2], assets[i * 2 + 1], {});
        if (!emitter) continue;
        emitter->position = position;
        emitter->field220 = emitter->field22C = {};
        emitter->fieldC4 = nullptr;
        emitter->fieldC8 = nullptr;
    }
}

particles::Emitter* particles::EffectFactory::createDamage(unsigned type, const std::array<float, 3>& position,
    SuctionCallback& callback) {
    for (unsigned i = 49; i > 0; --i) history[i] = history[i - 1];
    history[0] = type + 200;
    callback.capture = 0;
    const auto parameters = resources.get(0x4b485f45, "heart2ob.pcr");
    const auto texture = resources.get(0x4b485f45, "heart_ob.bti");
    const auto alternate = resources.get(0x4b485f45, "ps_ball.bti");
    auto* emitter = pool.create(parameters, texture, alternate);
    if (emitter) {
        emitter->position = position;
        emitter->field220 = {};
        emitter->field22C = {};
        emitter->fieldC4 = 0;
        emitter->fieldC8 = &callback;
    }
    return emitter;
}

particles::Emitter* particles::EffectFactory::createLifeLoss(unsigned type, const std::array<float, 3>& position,
    std::int32_t amount) {
    for (unsigned i = 49; i > 0; --i) history[i] = history[i - 1];
    history[0] = type + 210;
    const auto constants = range(executable, 0x804dcb78, 8);
    float base, multiplier;
    auto bits = word(constants, 0);
    std::memcpy(&base, &bits, sizeof(base));
    bits = word(constants, 4);
    std::memcpy(&multiplier, &bits, sizeof(multiplier));
    const auto absolute = amount < 0 ? 0u - static_cast<std::uint32_t>(amount) : static_cast<std::uint32_t>(amount);
    const float scale = base + multiplier * static_cast<float>(static_cast<std::int32_t>(absolute));
    const auto parameters = resources.get(0x4b485f45, "heart2.pcr");
    const auto texture = resources.get(0x4b485f45, "heart_32.bti");
    const auto alternate = resources.get(0x4b485f45, "ps_ball.bti");
    auto* emitter = pool.create(parameters, texture, alternate);
    if (emitter) {
        emitter->position = position;
        emitter->field220 = emitter->field22C = {};
        emitter->fieldC4 = nullptr;
        emitter->fieldC8 = nullptr;
        std::memcpy(&emitter->parameters.fields10C[8], &scale, sizeof(scale));
    }
    return emitter;
}

particles::Emitter* particles::EffectFactory::create(unsigned type, const std::array<float, 3>& position,
    const std::array<float, 3>& vector) {
    for (unsigned i = 49; i > 0; --i) history[i] = history[i - 1];
    history[0] = type;
    if (type > (0xffffffffu - 0x803825e8u) / 12) throw std::out_of_range("Original particle effect type is invalid.");
    const auto record = range(executable, 0x803825e8 + type * 12, 12);
    std::array<Resource, 3> data;
    for (unsigned i = 0; i < data.size(); ++i) {
        const auto address = word(record, i * 4);
        if (address) data[i] = resources.get(0x4b485f45, name(executable, address));
    }
    auto* emitter = pool.create(data[0], data[1], data[2]);
    if (emitter) {
        emitter->position = position;
        emitter->field220 = vector;
        const auto initial = range(executable, 0x8021da40, 12);
        for (unsigned i = 0; i < 3; ++i) {
            const auto bits = word(initial, i * 4);
            std::memcpy(&emitter->field22C[i], &bits, sizeof(bits));
        }
        emitter->fieldC4 = 0;
        emitter->fieldC8 = 0;
    }
    return emitter;
}
