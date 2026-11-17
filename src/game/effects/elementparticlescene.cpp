#include "resources/dolresources.h"
#include "game/effects/elementparticlescene.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    std::uint32_t word(const std::uint8_t* bytes) {
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float parameter(const elements::Emitter& emitter, unsigned offset) {
        const auto bits = word(emitter.parameters.data() + offset);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    int half(const elements::Emitter& emitter, unsigned offset) {
        return static_cast<std::int16_t>((unsigned(emitter.parameters[offset]) << 8) | emitter.parameters[offset + 1]);
    }

    float length(const std::array<float, 3>& value) {
        return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    }

    std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
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
    contextScale(scalar(executable, 0x804dc2d0)) {
    std::string error;
    if (!trig.read(executable, error)) throw std::runtime_error(error);
    const auto sineBytes = range(executable, 0x80218238, 4096 * 4);
    for (unsigned i = 0; i < sine.size(); ++i) {
        const auto bits = word(sineBytes.data() + i * 4);
        std::memcpy(&sine[i], &bits, 4);
    }
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned axis = 0; axis < 3; ++axis) corners[i][axis] = scalar(executable, 0x80377d00 + i * 12 + axis * 4);
        for (unsigned axis = 0; axis < 2; ++axis) coordinates[i][axis] = scalar(executable, 0x80377d30 + i * 8 + axis * 4);
    }
    auto texture = std::make_shared<std::vector<std::uint8_t>>(32);
    (*texture)[0] = 1;
    (*texture)[3] = (*texture)[5] = 32;
    const auto pointer = range(executable, 0x803767ac, 4);
    const auto image = range(executable, word(pointer.data()), 1024);
    texture->insert(texture->end(), image.begin(), image.end());
    blowImage = texture;
    blowThreshold = scalar(executable, 0x804d5934);
    timerScale = scalar(executable, 0x804d5930);
    timerBase = scalar(executable, 0x804dc2b0);
    timerControl = scalar(executable, 0x804dc2b4);
    collisionThreshold = scalar(executable, 0x804dc284);
    collisionScale = scalar(executable, 0x804dc288);
    radiansPerDegree = scalar(executable, 0x804dc2bc);
    particleScale = scalar(executable, 0x804dc2d4);
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
    if (type == 9) blow = &emitter;
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
        if (player.field84C >= 1) throw std::runtime_error("Active player breath emission requires its original particle update.");
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
    refreshForceRanges(context);
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

void elements::Scene::refreshForceRanges(particles::SceneContext& context) const {
    context.distance = context.suction.strength * contextScale;
    context.field44 = context.control * contextScale;
}

void elements::Scene::updateBlow(const playeractor::View& player, std::uint32_t actorFlags,
    particles::SceneContext& context, const mapcollision::State& collision, std::uint32_t& randomState) {
    if (paused || !blow) return;
    auto& emitter = *blow;
    auto random = [&] {
        randomState = randomState * 0x19660d + 0x3c6ef35f;
        return static_cast<float>(static_cast<double>(randomState) / 4294967296.0);
    };
    if (!player.fieldF15 && !(actorFlags & 0x10) && player.field8A0 == 6) {
        emitter.field3D = context.visible;
        if (context.visible && context.control > 0) {
            const float strength = context.control;
            for (unsigned axis = 0; axis < 3; ++axis) {
                emitter.position[axis] = (*context.suction.position)[axis] - (*player.velocity)[axis];
                emitter.velocity[axis] = (*context.suction.direction)[axis] * (strength * parameter(emitter, 0x14)) + (*player.velocity)[axis];
            }
            for (unsigned i = 0; i < emitter.parameters[0x31] && pool.particles.free.count; ++i) {
                auto& particle = *pool.particles.free.sentinel.previous;
                pool.particles.free.remove(particle);
                emitter.particles.append(particle);
                particle.position = emitter.position;
                for (unsigned axis = 0; axis < 3; ++axis)
                    particle.velocity[axis] = emitter.velocity[axis] + strength * parameter(emitter, axis * 4) * (1 - 2 * random());
                particle.acceleration.fill(0);
                particle.influenced = false;
                particle.age = static_cast<std::int16_t>(half(emitter, 0x10));
                particle.pulse = static_cast<std::uint16_t>(int(65535 * random()));
                particle.rotationSpeed = parameter(emitter, 0x40) * (1 - 2 * random());
                particle.rotation = 360 * random();
                particle.timer = (1 + timerScale * (timerBase - timerControl * strength)) * random();
                particle.maximumSize = strength * parameter(emitter, 0x0c);
                particle.baseSize = strength >= blowThreshold ? strength * parameter(emitter, 0x74) : 0;
                particle.growth = 0;
            }
        }
    }
    if (!emitter.field3D) return;
    for (auto* particle = emitter.particles.sentinel.next; particle != &emitter.particles.sentinel;) {
        auto* next = particle->next;
        if (particle->baseSize > particle->maximumSize || particle->influenced) particle->growth = 2;
        else particle->baseSize += parameter(emitter, 0x18);
        particle->pulse = static_cast<std::uint16_t>(int(float(particle->pulse) + parameter(emitter, 0x48)));
        const float amplitude = parameter(emitter, 0x44);
        particle->size = particle->baseSize * ((1 - amplitude) + amplitude * sine[particle->pulse >> 4]);
        particle->rotation += particle->rotationSpeed;
        particle->acceleration[1] -= parameter(emitter, 0x1c);
        const float damping = parameter(emitter, length(particle->velocity) <= parameter(emitter, 0x14) ? 0x20 : 0x24);
        for (unsigned axis = 0; axis < 3; ++axis) {
            particle->velocity[axis] = particle->velocity[axis] * damping + particle->acceleration[axis];
            particle->position[axis] += particle->velocity[axis];
        }
        if (--particle->age < 1) {
            emitter.particles.remove(*particle);
            pool.particles.free.append(*particle);
        } else if (particle->size > collisionThreshold) {
            const auto contact = collision.sweepSphere(particle->position, particle->size * collisionScale, particle->velocity);
            if (contact.floor != -1 || contact.wall != -1)
                for (unsigned axis = 0; axis < 3; ++axis) particle->position[axis] += contact.correction[axis];
            if (contact.floor != -1) particle->velocity[1] = 0;
            const float speed = length(particle->velocity);
            if (contact.wall != -1 && speed != 0) {
                auto reverse = particle->velocity;
                for (auto& value : reverse) value /= -speed;
                particle->velocity = cross(cross(reverse, contact.wallNormal), contact.wallNormal);
                for (auto& value : particle->velocity) value *= speed;
            }
        }
        particle = next;
    }
}

particles::Billboard elements::Scene::blowMaterial() const {
    particles::Billboard result;
    result.texture = blowImage;
    result.mode = 7;
    result.blend = 0x14;
    result.depth = 0x0b;
    result.cull = 0;
    for (unsigned i = 0; i < 4; ++i) result.secondary[i] = blow->parameters[0x5e + i];
    return result;
}

std::vector<particles::Billboard> elements::Scene::prepareBlow(const std::array<float, 12>& view) const {
    std::vector<particles::Billboard> result;
    if (!blow || !blow->field3D) return result;
    const int fadeIn = half(*blow, 0x28), fadeOut = half(*blow, 0x2a), lifetime = half(*blow, 0x10);
    const float opacity = parameter(*blow, 0x2c);
    for (auto* particle = blow->particles.sentinel.next; particle != &blow->particles.sentinel; particle = particle->next) {
        auto item = blowMaterial();
        float alpha = 255 * opacity;
        if (fadeIn != 0 && particle->age > lifetime - fadeIn) alpha *= float(lifetime - particle->age) / fadeIn;
        else if (particle->age <= fadeOut) alpha = 255 * (opacity * particle->age / fadeOut);
        item.color[3] = static_cast<std::uint8_t>(int(alpha));
        const auto rotation = trig.sinCos(particle->rotation * radiansPerDegree);
        for (unsigned i = 0; i < 4; ++i) {
            for (unsigned axis = 0; axis < 3; ++axis) item.positions[i][axis] = particle->position[axis] + particle->size *
                (corners[i][0] * view[axis] + corners[i][1] * view[4 + axis]);
            const float x = coordinates[i][0] - 0.5f, y = coordinates[i][1] - 0.5f;
            item.coordinates[i] = {0.5f + rotation[1] * x - rotation[0] * y, 0.5f + rotation[0] * x + rotation[1] * y};
        }
        result.push_back(std::move(item));
    }
    return result;
}
