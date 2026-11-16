#include "game/effects/runningdustmanager.h"
#include "resources/dolresources.h"
#include "game/math/originalfloattrig.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <optional>
#include <stdexcept>

namespace {
    constexpr unsigned parameterAddress = 0x80377cd8;
    constexpr unsigned textureAddress = 0x80377900;
    constexpr unsigned verticesAddress = 0x80377d00;
    constexpr unsigned coordinatesAddress = 0x80377d30;
    constexpr unsigned colorAddress = 0x80377dde;
    constexpr unsigned logarithmsAddress = 0x80217c38;
    constexpr unsigned reciprocalsAddress = 0x80217e3c;
    constexpr unsigned logarithmConstantsAddress = 0x804dd918;
    constexpr unsigned degreesToRadiansAddress = 0x804dc338;
    constexpr unsigned impactSpeedScaleAddress = 0x804dc33c;
    constexpr unsigned impactRadiusAddress = 0x804d5a00;
    constexpr unsigned particleCapacity = 1500, emitterCapacity = 80;
    using Vector = std::array<float, 3>;

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    }

    template<class T> T read(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        std::array<std::uint8_t, sizeof(T)> reversed;
        for (unsigned i = 0; i < sizeof(T); ++i) reversed[i] = bytes.at(offset + sizeof(T) - 1 - i);
        T result;
        std::memcpy(&result, reversed.data(), sizeof(T));
        return result;
    }

    float magnitude(const Vector& value) {
        return std::sqrt(std::fma(value[2], value[2], value[0] * value[0]) + value[1] * value[1]);
    }

    struct Parameters {
        float radialSpread, upwardSpread, maximumSize, speedThreshold, growthStep, slowDamping, fastDamping;
        float width, depth, rotationSpeed, pulseAmplitude, pulseStep, collisionVelocity, movementVelocity;
        float drift, driftStep, growthIncrement, forwardOffset, heightOffset, horizontalOffset;
        float upwardScale, upwardBase, radialScale, growthFraction, logarithmScale, logarithmBase;
        float accelerationScale, velocityScale;
        std::int16_t releasedLifetime;
        std::uint16_t lifetime;
        std::uint8_t alphaBase, alphaLimit, ageStep, captureDelay;

        explicit Parameters(const std::vector<std::uint8_t>& bytes) :
            radialSpread(read<float>(bytes, 0)), upwardSpread(read<float>(bytes, 4)), maximumSize(read<float>(bytes, 12)),
            speedThreshold(read<float>(bytes, 20)), growthStep(read<float>(bytes, 24)),
            slowDamping(read<float>(bytes, 32)), fastDamping(read<float>(bytes, 36)),
            width(read<float>(bytes, 52)), depth(read<float>(bytes, 60)), rotationSpeed(read<float>(bytes, 64)),
            pulseAmplitude(read<float>(bytes, 68)), pulseStep(read<float>(bytes, 72)),
            collisionVelocity(read<float>(bytes, 76)), movementVelocity(read<float>(bytes, 80)),
            drift(read<float>(bytes, 84)), driftStep(read<float>(bytes, 88)), growthIncrement(read<float>(bytes, 116)),
            forwardOffset(read<float>(bytes, 124)), heightOffset(read<float>(bytes, 128)),
            horizontalOffset(read<float>(bytes, 132)), upwardScale(read<float>(bytes, 136)),
            upwardBase(read<float>(bytes, 140)), radialScale(read<float>(bytes, 144)), growthFraction(read<float>(bytes, 148)),
            logarithmScale(read<float>(bytes, 152)), logarithmBase(read<float>(bytes, 156)),
            accelerationScale(read<float>(bytes, 104)), velocityScale(read<float>(bytes, 108)),
            releasedLifetime(read<std::int16_t>(bytes, 42)), lifetime(read<std::uint16_t>(bytes, 16)),
            alphaBase(bytes[92]), alphaLimit(bytes[93]), ageStep(bytes[120]), captureDelay(bytes[113]) {}
    };

    struct Particle : runningdust::InteractionParticle {
        Particle* next = this;
        Particle* previous = this;
        float driftAngle = 0, driftSign = 1;
        unsigned growthPhase = 0;
    };

    struct Emitter {
        Emitter* next = this;
        Emitter* previous = this;
        particles::Ring<Particle> particles;
        particles::SceneContext* context = nullptr;
        const Parameters* parameters = nullptr;
        std::optional<Parameters> localParameters;
        Vector origin{};
        float floor = 0, logarithmInput = 0, growthThreshold = 0, collisionRadius = 0;
        unsigned room = 0, level = 0;
        bool visible = true;
    };

    struct Region {
        int row;
        Vector position;
        std::uint32_t radius, level;
    };

    template<class T> T* takeLast(particles::Ring<T>& list) {
        if (!list.count) return nullptr;
        auto* result = list.sentinel.previous;
        list.remove(*result);
        return result;
    }
}

struct runningdust::Manager::State {
    State(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
        const roomlocation::Rooms& rooms, const roomtraversal::State& visibility, std::uint32_t& randomState) :
        rooms(rooms), visibility(visibility), randomState(randomState) {
        std::string error;
        if (!trig.read(executable, error)) throw std::runtime_error(error);
        radiansPerDegree = read<float>(range(executable, degreesToRadiansAddress, 4), 0);
        impactSpeedScale = read<float>(range(executable, impactSpeedScaleAddress, 4), 0);
        releasedDecay = read<float>(range(executable, 0x804dc2e8, 4), 0);
        releasedMinimum = read<float>(range(executable, 0x804dc2ec, 4), 0);
        const auto impact = range(executable, impactRadiusAddress, 8);
        impactRadius = read<float>(impact, 0);
        impactDensity = read<float>(impact, 4);
        for (unsigned level = 0; level < 6; ++level)
            parameters.emplace_back(range(executable, parameterAddress + level * 168, 168));
        if (const auto* table = mission.getJmpResource("RoomInfo")) {
            for (unsigned row = 0; row < table->count(); ++row) {
                std::uint32_t room, level;
                if (!table->getValue(row, "RoomNo", room) || !table->getValue(row, "DustLv", level))
                    throw std::runtime_error("Running dust requires the original RoomInfo fields.");
                levels.at(room) = static_cast<std::uint8_t>(level);
            }
        }
        auto bytes = range(executable, glme01::sineTable, 4096 * 4);
        for (unsigned i = 0; i < sine.size(); ++i) sine[i] = read<float>(bytes, i * 4);
        bytes = range(executable, logarithmsAddress, 129 * 4);
        for (unsigned i = 0; i < logarithms.size(); ++i) logarithms[i] = read<float>(bytes, i * 4);
        bytes = range(executable, reciprocalsAddress, 129 * 4);
        for (unsigned i = 0; i < reciprocals.size(); ++i) reciprocals[i] = read<float>(bytes, i * 4);
        bytes = range(executable, logarithmConstantsAddress, 20);
        logQuadratic = read<float>(bytes, 0);
        logCubic = read<float>(bytes, 4);
        logTwo = read<float>(bytes, 16);
        bytes = range(executable, verticesAddress, 48);
        for (unsigned i = 0; i < 4; ++i) for (unsigned axis = 0; axis < 3; ++axis)
            corners[i][axis] = read<float>(bytes, i * 12 + axis * 4);
        bytes = range(executable, coordinatesAddress, 32);
        for (unsigned i = 0; i < 4; ++i) for (unsigned axis = 0; axis < 2; ++axis)
            coordinates[i][axis] = read<float>(bytes, i * 8 + axis * 4);
        bytes = range(executable, colorAddress, 4);
        std::copy(bytes.begin(), bytes.end(), color.begin());
        auto texture = std::make_shared<std::vector<std::uint8_t>>(32);
        (*texture)[0] = 1;
        (*texture)[3] = (*texture)[5] = 32;
        bytes = range(executable, textureAddress, 1024);
        texture->insert(texture->end(), bytes.begin(), bytes.end());
        image = texture;
    }

    float random() {
        randomState = randomState * 0x19660d + 0x3c6ef35f;
        return float(randomState) / 4294967296.0f;
    }

    float logarithm(float value) const {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        const int exponent = int(bits >> 23) - 127;
        unsigned index = (bits >> 16) & 127;
        if (!(bits & 65535)) return logTwo * float(exponent) + logarithms[index];
        std::uint32_t baseBits = (bits & 0x7f0000) | 0x3f800000;
        if (bits & 0x8000) { ++index; baseBits += 0x10000; }
        float base, mantissa;
        std::memcpy(&base, &baseBits, 4);
        bits = (bits & 0x7fffff) | 0x3f800000;
        std::memcpy(&mantissa, &bits, 4);
        const float x = (mantissa - base) * reciprocals[index];
        return std::fma(x * x, std::fma(x, logCubic, logQuadratic),
            std::fma(logTwo, float(exponent), logarithms[index]) + x);
    }

    unsigned regionLevel(const Vector& position) const {
        float closest = 0;
        unsigned result = 0;
        bool first = true;
        for (const auto& region : regions) {
            Vector difference;
            for (unsigned i = 0; i < 3; ++i) difference[i] = position[i] - region.position[i];
            const float distance = magnitude(difference);
            if (first) closest = distance;
            if (distance <= float(region.radius) && (first || distance < closest)) {
                closest = distance;
                result = region.level;
            }
            first = false;
        }
        return result;
    }

    void emit(particles::SceneContext& context, unsigned modifier) {
        auto level = levels[context.room & 255];
        if (!context.actorPosition || context.field48 <= 0 || !level) return;
        auto* emitter = takeLast(emitters.free);
        if (!emitter) return;
        active.append(*emitter);
        level = static_cast<std::uint8_t>(std::min(5u, (unsigned(level) + modifier) & 255));
        const auto& p = parameters[level];
        emitter->context = &context;
        emitter->parameters = &p;
        emitter->level = level;
        emitter->room = context.room;
        emitter->visible = true;
        emitter->floor = float(rooms.entries().at(context.room >> 24).bounds[1]);
        emitter->logarithmInput = p.logarithmBase;
        emitter->growthThreshold = p.maximumSize * p.growthFraction;
        emitter->collisionRadius = 0;
        emitter->origin = *context.actorPosition;
        emitter->origin[0] += context.orientation[0] * p.forwardOffset;
        emitter->origin[0] += p.horizontalOffset;
        emitter->origin[1] += p.heightOffset;
        const unsigned count = context.field5C ? 3 : 6;
        const float width = p.width * (context.field5C ? 0.5f : 1.0f);
        const float depth = p.depth * (context.field5C ? 0.5f : 1.0f);
        const float stepX = width / float(count - 1), stepZ = depth / float(count - 1);
        const float radial = p.radialScale * context.magnitude;
        const float upward = p.upwardBase + p.upwardScale * context.magnitude;
        for (unsigned z = 0; z < count; ++z) for (unsigned x = 0; x < count; ++x) {
            Vector position = emitter->origin;
            position[0] += -0.5f * width + (0.5f * (float(x) * stepX)) * (1 + random());
            position[2] += -0.5f * depth + (0.5f * (float(z) * stepZ)) * (1 + random());
            auto* particle = takeLast(particles.free);
            if (!particle) continue;
            *particle = Particle{};
            emitter->particles.append(*particle);
            particle->position = position;
            Vector direction;
            for (unsigned i = 0; i < 3; ++i) direction[i] = position[i] - emitter->origin[i];
            const float length = magnitude(direction);
            if (length > 0) for (auto& value : direction) value /= length;
            const float speed = radial + p.radialSpread * (1 - 2 * random());
            for (unsigned i = 0; i < 3; ++i) particle->velocity[i] = direction[i] * speed;
            particle->velocity[1] = upward + p.upwardSpread * random();
            particle->lifetime = static_cast<std::int16_t>(p.lifetime);
            particle->pulse = static_cast<std::uint16_t>(int(65535 * random()));
            particle->rotationSpeed = p.rotationSpeed * (1 - 2 * random());
            particle->rotation = 360 * random();
            particle->driftSign = 1 - 2 * random() < 0 ? -1.0f : 1.0f;
        }
    }

    void update(Emitter& emitter, Particle& particle, const particles::SceneContext& player) {
        const auto& p = *emitter.parameters;
        if (particle.influenced) {
            particle.baseSize *= releasedDecay;
            if (particle.baseSize < releasedMinimum) particle.baseSize = releasedMinimum;
        } else if (particle.growthPhase != 2) {
            if (!particle.growthPhase) particle.baseSize = p.logarithmScale * logarithm(emitter.logarithmInput);
            else particle.baseSize += p.growthStep;
            if (particle.baseSize >= p.maximumSize) particle.growthPhase = 2;
            else if (particle.baseSize >= emitter.growthThreshold) particle.growthPhase = 1;
        }
        particle.pulse = static_cast<std::uint16_t>(int(float(particle.pulse) + p.pulseStep));
        particle.size = particle.baseSize * ((1 - p.pulseAmplitude) + p.pulseAmplitude * sine[particle.pulse >> 4]);
        particle.rotation += particle.rotationSpeed;
        particle.driftAngle += particle.driftSign * p.driftStep * (1 + 0.5f * random());
        particle.velocity[0] += p.drift * sine[(std::uint32_t(int(particle.driftAngle)) >> 4) & 4095];
        const float damping = magnitude(particle.velocity) <= p.speedThreshold ? p.slowDamping : p.fastDamping;
        for (unsigned i = 0; i < 3; ++i) {
            particle.velocity[i] *= damping;
            particle.velocity[i] *= p.slowDamping;
            particle.position[i] += particle.velocity[i];
        }
        const auto& context = *emitter.context;
        const auto& position = *context.actorPosition;
        Vector normal{particle.position[0] - position[0], 0, particle.position[2] - position[2]};
        const float distance = magnitude(normal);
        if (distance <= emitter.collisionRadius && (*player.actorPosition)[1] <= particle.position[1] &&
            particle.position[1] <= context.height) {
            if (distance > 0) for (auto& value : normal) value /= distance;
            particle.position[0] = position[0] + emitter.collisionRadius * normal[0];
            particle.position[2] = position[2] + emitter.collisionRadius * normal[2];
            const float speed = magnitude(particle.velocity);
            for (unsigned i = 0; i < 3; ++i) normal[i] -= context.orientation[i];
            normal[0] *= 1 - 2 * random();
            normal[2] *= 1 - 2 * random();
            const float push = p.collisionVelocity * speed + p.movementVelocity * context.magnitude;
            for (unsigned i = 0; i < 3; ++i) particle.velocity[i] = normal[i] * push;
        }
        particle.lifetime = static_cast<std::int16_t>(particle.lifetime - p.ageStep);
        if (particle.lifetime < 1) {
            emitter.particles.remove(particle);
            particles.free.append(particle);
        }
    }

    const roomlocation::Rooms& rooms;
    const roomtraversal::State& visibility;
    std::uint32_t& randomState;
    particles::Pool<Particle> particles{particleCapacity};
    particles::Pool<Emitter> emitters{emitterCapacity};
    particles::Ring<Emitter> active;
    std::vector<Parameters> parameters;
    std::array<std::uint8_t, 256> levels{};
    std::array<float, 4096> sine{};
    std::array<float, 129> logarithms{}, reciprocals{};
    float logQuadratic = 0, logCubic = 0, logTwo = 0, radiansPerDegree = 0;
    float impactSpeedScale = 0, impactRadius = 0, impactDensity = 0;
    float releasedDecay = 0, releasedMinimum = 0;
    originalmath::FloatTrig trig;
    std::array<Vector, 4> corners{};
    std::array<std::array<float, 2>, 4> coordinates{};
    std::array<std::uint8_t, 4> color{};
    particles::Resource image;
    std::list<Region> regions;
};

runningdust::Manager::Manager(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
    const roomlocation::Rooms& rooms, const roomtraversal::State& visibility, std::uint32_t& randomState) :
    state(std::make_unique<State>(executable, mission, rooms, visibility, randomState)) {}
runningdust::Manager::~Manager() = default;

bool runningdust::Manager::impact(float speed, float radius, const Vector& position,
    const Vector& velocity, const Vector& normal, unsigned level, particles::SceneContext& player,
    const std::function<Vector()>& direction) {
    auto* emitter = takeLast(state->emitters.free);
    if (!emitter) return false;
    state->active.append(*emitter);
    emitter->room = state->rooms.resolve(position);
    emitter->floor = float(state->rooms.entries().at(emitter->room >> 24).bounds[1]);
    level &= 255;
    if (!level) return false;
    if (level >= state->parameters.size()) throw std::runtime_error("Impact dust requires an original dust-level parameter record.");
    const auto& p = state->parameters[level];
    emitter->context = &player;
    emitter->parameters = &p;
    emitter->level = level;
    emitter->visible = true;
    emitter->origin = position;
    emitter->logarithmInput = 1;
    emitter->growthThreshold = p.maximumSize * p.growthFraction;
    emitter->collisionRadius = 0;
    radius = std::min(radius, state->impactRadius);
    const int side = static_cast<int>(radius * state->impactDensity);
    const float count = float(side) * float(side);
    const float scaledSpeed = float(double(state->impactSpeedScale) * double(speed));
    // The original overwrites the X-derived radial speed with the Z-derived value.
    const float radial = velocity[2] * float(double(p.radialScale) * double(scaledSpeed));
    const float upward = p.upwardBase + velocity[1] * float(double(p.upwardScale) * double(scaledSpeed));
    for (unsigned i = 0; float(i) < count; ++i) {
        if (!direction) throw std::runtime_error("Impact dust requires the original direction random stream.");
        const auto random = direction();
        const Vector cross{random[1] * normal[2] - random[2] * normal[1],
            random[2] * normal[0] - random[0] * normal[2], random[0] * normal[1] - random[1] * normal[0]};
        Vector source;
        for (unsigned axis = 0; axis < 3; ++axis) source[axis] = position[axis] + cross[axis] * radius;
        auto* particle = takeLast(state->particles.free);
        if (!particle) continue;
        *particle = Particle{};
        emitter->particles.append(*particle);
        particle->position = source;
        Vector offset;
        for (unsigned axis = 0; axis < 3; ++axis) offset[axis] = source[axis] - position[axis];
        const float length = magnitude(offset);
        if (!(length > 0)) throw std::runtime_error("Impact dust reached the original uninitialized zero-displacement direction.");
        for (auto& value : offset) value /= length;
        const float outward = radial + p.radialSpread * (1 - 2 * state->random());
        for (unsigned axis = 0; axis < 3; ++axis) particle->velocity[axis] = offset[axis] * outward;
        particle->velocity[1] = upward + p.upwardSpread * state->random();
        particle->lifetime = static_cast<std::int16_t>(p.lifetime);
        particle->pulse = static_cast<std::uint16_t>(int(65535 * state->random()));
        particle->rotationSpeed = p.rotationSpeed * (1 - 2 * state->random());
        particle->rotation = 360 * state->random();
        particle->driftSign = 1 - 2 * state->random() < 0 ? -1.0f : 1.0f;
    }
    std::vector<Particle*> sorted;
    for (auto* particle = emitter->particles.sentinel.next; particle != &emitter->particles.sentinel; particle = particle->next)
        sorted.push_back(particle);
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto* left, const auto* right) { return left->position[2] < right->position[2]; });
    for (auto* particle : sorted) { emitter->particles.remove(*particle); emitter->particles.append(*particle); }
    return true;
}

bool runningdust::Manager::release(const std::array<float, 3>& position, unsigned count, particles::SceneContext& player) {
    auto* emitter = takeLast(state->emitters.free);
    if (!emitter) return false;
    state->active.append(*emitter);
    emitter->room = state->rooms.resolve(position);
    emitter->floor = float(state->rooms.entries().at(emitter->room >> 24).bounds[1]);
    emitter->context = &player;
    emitter->localParameters = state->parameters[1];
    auto& p = *emitter->localParameters;
    p.radialSpread = p.upwardSpread = 0;
    p.captureDelay = 0;
    emitter->parameters = &p;
    emitter->level = 1;
    emitter->visible = true;
    emitter->origin = position;
    emitter->logarithmInput = 1;
    emitter->growthThreshold = p.maximumSize * p.growthFraction;
    emitter->collisionRadius = 0;
    for (unsigned i = 0; i < std::min(count & 255u, 36u); ++i) {
        auto* particle = takeLast(state->particles.free);
        if (!particle) continue;
        *particle = Particle{};
        emitter->particles.append(*particle);
        particle->position = position;
        state->random();
        state->random();
        particle->lifetime = static_cast<std::int16_t>(p.lifetime);
        particle->pulse = static_cast<std::uint16_t>(int(65535 * state->random()));
        particle->rotationSpeed = p.rotationSpeed * (1 - 2 * state->random());
        particle->rotation = 360 * state->random();
        particle->driftSign = 1 - 2 * state->random() < 0 ? -1.0f : 1.0f;
    }
    return true;
}

bool runningdust::Manager::create(jmp::ToolDataRef& entry) {
    std::string_view name;
    if (!entry.table || !entry.table->getValue(entry.row, "name", name) || name != "dust") return false;
    if (state->regions.size() == 30) return true;
    Region region{};
    region.row = entry.row;
    if (!entry.table->getValue(entry.row, "arg0", region.radius) || !entry.table->getValue(entry.row, "arg1", region.level))
        throw std::runtime_error("Original dust region parameters are missing.");
    const char* fields[] = {"pos_x", "pos_y", "pos_z"};
    for (unsigned i = 0; i < 3; ++i) if (!entry.table->getValue(entry.row, fields[i], region.position[i]))
        throw std::runtime_error("Original dust region position is missing.");
    state->regions.push_back(region);
    return true;
}

bool runningdust::Manager::remove(jmp::ToolDataRef& entry) {
    std::string_view name;
    if (!entry.table || !entry.table->getValue(entry.row, "name", name) || name != "dust") return false;
    const auto found = std::find_if(state->regions.begin(), state->regions.end(), [&](const auto& region) { return region.row == entry.row; });
    if (found != state->regions.end()) state->regions.erase(found);
    return true;
}

void runningdust::Manager::update(particles::SceneContexts& contexts, const ParticleInteraction& interaction) {
    if (!contexts.player.actorPosition) throw std::logic_error("Running dust requires the refreshed player context.");
    const auto modifier = state->regionLevel(*contexts.player.actorPosition);
    state->emit(contexts.player, modifier);
    if (state->levels[contexts.player.room & 255] || contexts.player.field48 <= 0) state->emit(contexts.secondary, modifier);
    auto& active = state->active;
    for (auto* emitter = active.sentinel.next; emitter != &active.sentinel;) {
        auto* next = emitter->next;
        if (!(state->visibility.record(emitter->room, 0).flags & 1)) emitter->visible = false;
        if (!emitter->particles.count) {
            active.remove(*emitter);
            state->emitters.free.append(*emitter);
        } else {
            emitter->logarithmInput += emitter->parameters->growthIncrement;
            emitter->collisionRadius = std::min(50.0f, emitter->collisionRadius + 1);
            for (auto* particle = emitter->particles.sentinel.next; particle != &emitter->particles.sentinel;) {
                auto* following = particle->next;
                const auto& p = *emitter->parameters;
                const EmitterInteraction parameters{emitter->level,
                    static_cast<std::int16_t>(p.lifetime - p.captureDelay), p.releasedLifetime,
                    {p.pulseAmplitude, p.pulseStep, p.accelerationScale, p.velocityScale}};
                if (!interaction && (particle->forced || particle->influenced))
                    throw std::logic_error("Influenced dust requires its retained interaction service.");
                const auto result = interaction ? interaction(*particle, parameters) : ForceResult::Ordinary;
                if (result == ForceResult::Removed) {
                    emitter->particles.remove(*particle);
                    state->particles.free.append(*particle);
                } else if (!particle->forced) state->update(*emitter, *particle, contexts.player);
                // The original clamps the next list entry before advancing to it.
                if (following != &emitter->particles.sentinel && following->position[1] <= emitter->floor) {
                    following->position[1] = emitter->floor;
                    following->velocity[1] = 0;
                }
                particle = following;
            }
        }
        emitter = next;
    }
    std::vector<Emitter*> sorted;
    for (auto* emitter = active.sentinel.next; emitter != &active.sentinel; emitter = emitter->next) sorted.push_back(emitter);
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto* left, const auto* right) { return left->origin[2] < right->origin[2]; });
    for (auto* emitter : sorted) { active.remove(*emitter); active.append(*emitter); }
}

particles::Billboard runningdust::Manager::material() const {
    particles::Billboard result;
    result.texture = state->image;
    result.mode = 5;
    result.blend = 0x54;
    result.depth = 0x0b;
    result.cull = 0;
    result.secondary = state->color;
    return result;
}

std::vector<particles::Billboard> runningdust::Manager::prepare(const std::array<float, 12>& view) const {
    std::vector<particles::Billboard> result;
    const auto& active = state->active;
    for (auto* emitter = active.sentinel.next; emitter != &active.sentinel; emitter = emitter->next) {
        if (!emitter->visible) continue;
        for (auto* particle = emitter->particles.sentinel.next; particle != &emitter->particles.sentinel; particle = particle->next) {
            auto item = material();
            const auto& parameters = *emitter->parameters;
            item.color[3] = static_cast<std::uint8_t>(parameters.alphaBase - std::min<unsigned>(particle->lifetime, parameters.alphaLimit));
            const auto rotation = state->trig.sinCos(particle->rotation * state->radiansPerDegree);
            const float sine = rotation[0], cosine = rotation[1];
            for (unsigned i = 0; i < 4; ++i) {
                for (unsigned axis = 0; axis < 3; ++axis) item.positions[i][axis] = particle->position[axis] + particle->size *
                    (state->corners[i][0] * view[axis] + state->corners[i][1] * view[4 + axis]);
                const float x = state->coordinates[i][0] - 0.5f, y = state->coordinates[i][1] - 0.5f;
                item.coordinates[i] = {0.5f + cosine * x - sine * y, 0.5f + sine * x + cosine * y};
            }
            result.push_back(std::move(item));
        }
    }
    return result;
}

unsigned runningdust::Manager::particleCount() const { return particleCapacity - state->particles.free.count; }
unsigned runningdust::Manager::emitterCount() const { return state->active.count; }
unsigned runningdust::Manager::level(unsigned room) const { return state->levels[room & 255]; }
