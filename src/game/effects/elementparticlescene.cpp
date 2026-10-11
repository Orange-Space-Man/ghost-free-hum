#include "resources/dolresources.h"
#include "game/effects/elementparticlescene.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bytes = range(executable, address, 4);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    std::array<float, 3> vector(const std::vector<std::uint8_t>& executable, unsigned address) {
        return {scalar(executable, address), scalar(executable, address + 4), scalar(executable, address + 8)};
    }
}

elements::Scene::Scene(const std::vector<std::uint8_t>& executable) : pool(1500, 15), executable(executable), rooms(executable, 2),
    contextScale(scalar(executable, 0x804dc250)) {
    particleScale = scalar(executable, 0x804dc254);
    ice.field98 = 1;
    ice.fieldA8 = 0x2f;
    ice.fieldAC = 0x30;
    ice.fieldB0 = 0x10;
    water.fieldA4 = 1;
    water.fieldA8 = 0x31;
    water.fieldAC = 0x32;
    water.fieldB0 = 0x11;
    const auto firstTranslation = scalar(executable, 0x804dc2a4);
    const auto secondTranslation = scalar(executable, 0x804dc280);
    firstTextureMatrix = {1, 0, 0, firstTranslation, 0, 1, 0, firstTranslation, 0, 0, 1, 0};
    secondTextureMatrix = {1, 0, 0, secondTranslation, 0, 1, 0, secondTranslation, 0, 0, 1, 0};
    const auto zero = vector(executable, 0x8021da40);
    auto position = vector(executable, 0x8020cc0c);
    position[1] = scalar(executable, 0x804dc2d8) + scalar(executable, 0x804d5938);
    const auto direction = vector(executable, 0x8020cc18);
    initialize(0, zero, zero);
    initialize(10, zero, zero);
    initialize(9, position, direction);
    initialize(11, zero, zero);
    initialize(2, position, direction);
    const auto upward = vector(executable, 0x8020cc24);
    for (unsigned i = 0; i < 4; ++i) initialize(8, vector(executable, 0x8020cc30 + i * 12), upward);
}

void elements::Scene::initialize(unsigned type, const std::array<float, 3>& position, const std::array<float, 3>& direction) {
    if (!pool.emitters.free.count) return;
    auto& emitter = *pool.emitters.free.sentinel.previous;
    pool.emitters.free.remove(emitter);
    pool.active.append(emitter);
    emitter.type = type;
    const auto bytes = range(executable, 0x80376808 + type * 168, 168);
    std::copy(bytes.begin(), bytes.end(), emitter.parameters.begin());
    emitter.field2C = static_cast<std::uint16_t>(((unsigned(bytes[0x10]) << 8) | bytes[0x11]) - bytes[0x71]);
    emitter.room = rooms.resolve(position);
    const auto& bounds = rooms.entries().at(emitter.room >> 24).bounds;
    for (unsigned i = 0; i < 3; ++i) {
        emitter.minimum[i] = static_cast<float>(bounds[i]);
        emitter.maximum[i] = static_cast<float>(bounds[i + 3]);
    }
    emitter.field28 = 0;
    emitter.field3C = 0;
    emitter.field1B4 = bytes[0x31];
    const unsigned flags = bytes[0x70];
    emitter.motion = 0x80129880;
    if (type >= 9 && type <= 12) emitter.initializer = 0x80128d58;
    else {
        if (flags & 1) emitter.motion = 0x8012962c;
        else if (flags & 0x10) emitter.motion = 0x80129770;
        emitter.initializer = flags & 4 ? 0x80128424 : flags & 2 ? 0x801288f4 : flags & 8 ? 0x8012867c :
            flags & 0x20 ? 0x80128ad8 : 0x80128424;
    }
    emitter.progression = type == 2 || type == 8 ? 0x8012a0dc : 0x80129e24;
    emitter.control = type == 0 ? 0x8012a904 : type == 9 ? 0x8012aebc : type == 10 ? 0x8012ac14 :
        type == 11 ? 0x8012ad4c : 0x8012ab24;
    emitter.position = position;
    emitter.direction = direction;
}

void elements::Scene::control(Emitter& emitter, const playeractor::View& player, std::uint32_t actorFlags,
    particles::SceneContext& context, const roomtraversal::State& visibility) {
    if (emitter.type == 0) {
        const auto bytes = range(executable, 0x80376808, 168);
        std::copy(bytes.begin(), bytes.end(), emitter.parameters.begin());
        emitter.field38 = 0;
        emitter.field3D = 1;
        if (!context.field38) throw std::runtime_error("Elemental player emitter requires its original position source.");
        emitter.position = *context.field38;
        if (player.field84C >= 1) throw std::runtime_error("Active vacuum particle emission is outside first-area loading.");
    } else if (emitter.type == 9 || emitter.type == 10 || emitter.type == 11) {
        if (player.fieldF15 || (actorFlags & 0x10)) return;
        const unsigned mode = emitter.type == 9 ? 6 : emitter.type == 10 ? 4 : 2;
        const bool available = emitter.type == 9 || (emitter.type == 10 ? ice.active : water.active) == 0;
        if (available && player.field8A0 == mode) {
            throw std::runtime_error("Active elemental weapon emission is outside first-area loading.");
        }
    } else {
        emitter.field38 = 1;
        emitter.field3D = (visibility.record(emitter.room, 0).flags & 1) != 0;
        if (!emitter.field3D) return;
        throw std::runtime_error("Basement elemental emission requires its room simulation dependencies.");
    }
}

void elements::Scene::updateProjectiles(const playeractor::View& player, std::uint32_t actorFlags) {
    if (!player.controls) throw std::runtime_error("Elemental scene requires original player controls.");
    if (!(player.controls->fieldCC && (player.fieldF15 || (actorFlags & 0x10)))) {
        if ((player.controls->fieldCC && (player.field8A0 == 2 || player.field8A0 == 4)) || ice.active || water.active) {
            throw std::runtime_error("Active elemental projectiles are outside first-area loading.");
        }
    }
}

void elements::Scene::update(const playeractor::View& player, std::uint32_t actorFlags,
    particles::SceneContext& context, const roomtraversal::State& visibility) {
    for (auto* emitter = pool.active.sentinel.next; emitter != &pool.active.sentinel; emitter = emitter->next) {
        if (emitter->particles.count) throw std::runtime_error("Active elemental collision and particle simulation require their gameplay dependencies.");
    }
    if (paused) return;
    context.distance = context.suction.strength * contextScale;
    context.field44 = context.control * contextScale;
    for (auto* emitter = pool.active.sentinel.next; emitter != &pool.active.sentinel;) {
        auto* next = emitter->next;
        control(*emitter, player, actorFlags, context, visibility);
        if (emitter->field3C && !emitter->particles.count) {
            pool.active.remove(*emitter);
            pool.emitters.free.append(*emitter);
        }
        emitter = next;
    }
}
