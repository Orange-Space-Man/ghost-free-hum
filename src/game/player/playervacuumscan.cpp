#include "game/player/playervacuumscan.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    using vacuumcapture::Vector;

    constexpr float smoothing = 0.2f;        // FLOAT_804DAB2C
    constexpr double scanRadius = 150.0;     // FLOAT_804DAB80
    constexpr float drainFactor = 3.0f;      // FLOAT_804DAB84
    constexpr float lifeStep = 0.0f;         // FLOAT_804DAB20 (also the lower clamp)
    constexpr unsigned eventVisible = 1;     // DAT_804D2074
    constexpr unsigned eventCapturable = 0x1a; // DAT_804D2078
    constexpr unsigned eventRelease = 4;     // DAT_804D207C

    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }

    Vector transform(const actormodel::Matrix& matrix, const Vector& value) {
        Vector result;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = row * 4;
            const float xy = std::fma(matrix[offset], value[0], matrix[offset + 1] * value[1]);
            result[row] = xy + std::fma(matrix[offset + 2], value[2], matrix[offset + 3]);
        }
        return result;
    }

    // 800BE4B8: world position of a collision sphere (same expression as ghostcapture::Collision::sphereCenter, which is private).
    Vector sphereCenter(const actors::MoveActor& actor, const actormodel::InitialPose& pose, const actorcollision::Record& record) {
        const Vector local{number(record[2]), number(record[3]), number(record[4])};
        const auto joint = static_cast<std::int32_t>(record[0]);
        if (joint < 0) {
            auto matrix = pose.rotation(actor.rotation);
            for (unsigned i = 0; i < 3; ++i) matrix[i * 4 + 3] = actor.position[i];
            return transform(matrix, local);
        }
        if (!actor.model) return {};
        return transform(actor.matrix, transform(actor.model->matrix(0, static_cast<unsigned>(joint)), local));
    }

    bool allowedType(unsigned type) { return type == 0xde || type == 0xdd; } // 800E9F5C list at 804DBB08
}

vacuumcapture::Vacuum::Vacuum(ScanServices services, const playermodels::VacuumVolume& beam, std::uint32_t debugFlags) :
    services_(std::move(services)), beam_(beam), debugFlags_(debugFlags) {
    if (!services_.candidates || !services_.typeGroup || !services_.typeLife || !services_.roomVisible || !services_.sound ||
        !services_.loopSound || !services_.particle || !services_.beginBeam || !services_.updateBeam || !services_.ownerEvent)
        throw std::runtime_error("Original vacuum object requires its manager, sound, beam and owner services.");
}

void vacuumcapture::Vacuum::attach(actors::MoveActor& actor) {
    for (auto& slot : attached) {
        if (!slot) {
            slot = &actor;
            return;
        }
    }
    throw std::runtime_error("Original vacuum attachment list is full.");
}

bool vacuumcapture::Vacuum::inside(const actors::MoveActor& actor, const playermodels::VacuumVolume& volume) const {
    // 8008ADC0: the volume is tested against every collision sphere of the actor's type, grown by 150.
    Vector direction{};
    for (const auto& record : services_.shapes.type(actor.type()).records) {
        const auto center = sphereCenter(actor, services_.pose, record);
        const auto radius = static_cast<float>(static_cast<double>(number(record[1])) + scanRadius);
        if (volume.intersects(radius, direction, center)) return true;
    }
    return false;
}

unsigned vacuumcapture::Vacuum::admit(const Candidate& candidate) {
    // 800849EC.
    auto& target = *candidate.actor;
    const unsigned type = target.type();
    auto& source = *input.player;
    unsigned result = 0;
    if (services_.typeGroup(type) < 2) {
        if (allowedType(type) && target.life == 0 && services_.entries.add(source, target, 0)) services_.sound(0x281b, nullptr);
        return 0;
    }
    unsigned heavy;
    const auto life = static_cast<std::int16_t>(target.life);
    if ((debugFlags_ & 4) == 0) {
        heavy = (life == 0 ? 1u : 0u) - (life < 0 ? 0xffffffffu : 0u);
    } else {
        const auto limit = static_cast<std::uint32_t>(services_.typeLife(type));
        heavy = (static_cast<std::int32_t>(limit) >> 31) +
            ((static_cast<std::uint32_t>(static_cast<std::int32_t>(life)) <= limit ? 1u : 0u) - (life < 0 ? 0xffffffffu : 0u));
    }
    if ((heavy & 0xff) == 0) {
        float remaining = static_cast<float>(static_cast<double>(static_cast<float>(life)) -
            static_cast<double>(drainFactor * strength));
        if (remaining < lifeStep) remaining = lifeStep;
        target.life = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int32_t>(remaining)));
        services_.sound(0x1800, &target.position);
    } else if (((debugFlags_ & 1) != 0 || !services_.entries.containsSource(&source)) && services_.entries.add(source, target, 1)) {
        services_.sound(0x1810, &target.position);
        services_.sound(0x281d, nullptr);
        result = 1;
    }
    return result;
}

unsigned vacuumcapture::Vacuum::scan() {
    // 80084844 / 8008487C.
    const playermodels::VacuumVolume& volume = input.volume ? *input.volume : beam_;
    unsigned result = 0;
    for (const auto& candidate : services_.candidates()) {
        auto& actor = *candidate.actor;
        if (services_.typeGroup(actor.type()) < 0) continue;
        const unsigned room = (actor.fieldB4 >> 24) & 0xff;
        if (!services_.roomVisible(room, 0) && !services_.roomVisible(room, 1)) continue;
        if (!inside(actor, volume)) continue;
        if (debugFlags_ & 8) {
            characterevent::Event visible(eventVisible);
            candidate.event(visible);
        }
        characterevent::Event capturable(eventCapturable);
        if ((candidate.event(capturable) & 0xff) != 0) result |= admit(candidate);
    }
    return result;
}

void vacuumcapture::Vacuum::releaseAttached() {
    // 80084BC0.
    bool any = false;
    const auto room = input.player->fieldB4;
    for (auto& slot : attached) {
        if (!slot) continue;
        any = true;
        if (!(slot->flags & 0x80000) || room != slot->fieldB4) {
            characterevent::Event release(eventRelease);
            services_.ownerEvent(*slot, release);
            slot = nullptr;
            services_.sound(0x804, &input.player->position);
            services_.particle(*input.hand);
        } else {
            slot->field100 = *input.hand;
        }
    }
    if (any) services_.sound(0x1033, &input.player->position);
}

unsigned vacuumcapture::Vacuum::update(const VacuumInput& value) {
    // 80084550.
    input = value;
    if (!input.player || !input.hand) throw std::runtime_error("Original vacuum update requires its player and hand position.");
    strength = strength + smoothing * (input.strength - strength);
    if (!active || strength != 0.0f) {
        if (!active && 0.0f < strength) {
            // 800847E8.
            active = true;
            started = true;
            services_.beginBeam(input);
            strength = 0.0f;
        }
    } else {
        active = false;
    }
    unsigned result = 0;
    if (active) {
        if (!input.volume) services_.updateBeam(strength, input); // 80084CE8 only runs without the volume override
        if (input.scan) result = scan();
        unsigned loop = 1;
        if (input.mode == 3) {
            if (!holding) {
                holding = true;
                loop = 5;
            }
        } else if (holding) {
            holding = false;
            loop = 3;
        }
        services_.loopSound(strength, input.player->position, loop);
    }
    releaseAttached();
    return result;
}

void vacuumcapture::Vacuum::reset() {
    // 80084268.
    strength = 0.0f;
    active = false;
}
