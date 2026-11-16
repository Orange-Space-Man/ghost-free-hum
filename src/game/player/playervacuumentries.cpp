#include "game/player/playervacuumentries.h"
#include "game/player/playervacuummodel.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using vacuumcapture::Vector;

    // Constants read from the DOL (804DAAB8.., 804D2028..) and the static initializers.
    constexpr std::int32_t sourceWord2 = 900;     // DAT_804D202C
    constexpr int shortWindow = 45;               // DAT_804D2030
    constexpr int resetWindow = 30;               // DAT_804D2034
    constexpr float resetScale = 20.0f;           // FLOAT_804D2038
    constexpr std::uint32_t caughtLimit = 10;     // DAT_804D203C
    constexpr float maximumSpeed = 40.0f;         // FLOAT_804D2040
    constexpr float nearRange = 200.0f;           // FLOAT_804D2044
    constexpr float farRange = 350.0f;            // FLOAT_804D2048 (also FLOAT_804D8C04, written by 80082790)
    constexpr float releaseOffset = -7.0f;        // FLOAT_804D204C
    constexpr float releaseFrames = 10.0f;        // DAT_804D2050
    constexpr float drainStep = -0.005f;          // FLOAT_804D2054
    constexpr float reducedWindow = 10.0f;        // DAT_804D2058 / DAT_804D205C
    constexpr unsigned reducedModulus = 1;        // DAT_804D2060
    constexpr bool coneAssist = true;             // DAT_804D2064
    constexpr std::uint32_t releaseEventKind = 1; // DAT_804D206C
    constexpr float globalFactor = 0.0f;          // FLOAT_804D8C00 (never written)
    constexpr float beamDistance = 300.0f;        // FLOAT_804DAB04

    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    Vector plus(const Vector& a, const Vector& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
    Vector scale(const Vector& v, double factor) {
        return {static_cast<float>(static_cast<double>(v[0]) * factor), static_cast<float>(static_cast<double>(v[1]) * factor),
            static_cast<float>(static_cast<double>(v[2]) * factor)};
    }
    double dot(const Vector& a, const Vector& b) {
        return static_cast<double>(a[0]) * static_cast<double>(b[0]) + static_cast<double>(a[1]) * static_cast<double>(b[1]) +
            static_cast<double>(a[2]) * static_cast<double>(b[2]);
    }
    double lengthSquared(const Vector& v) {
        return static_cast<double>(v[2]) * static_cast<double>(v[2]) + static_cast<double>(v[0]) * static_cast<double>(v[0]) +
            static_cast<double>(v[1]) * static_cast<double>(v[1]);
    }
    // 801E26D0 / 801E28B0: square root through three Newton steps (modelled with the exact root; Gekko estimate deferred).
    double root(double squared) { return squared > 0.0 ? static_cast<double>(static_cast<float>(std::sqrt(squared))) : squared; }
    double length(const Vector& v) { return root(lengthSquared(v)); }
    double distanceBetween(const Vector& a, const Vector& b) {
        const double x = static_cast<double>(a[0]) - static_cast<double>(b[0]);
        const double y = static_cast<double>(a[1]) - static_cast<double>(b[1]);
        const double z = static_cast<double>(a[2]) - static_cast<double>(b[2]);
        return root(x * x + y * y + z * z);
    }
    // 800781F8.
    Vector normalize(const Vector& v) {
        const double size = length(v);
        if (1.0E-4f <= size) return scale(v, static_cast<double>(static_cast<float>(1.0 / size)));
        return {0.0f, 0.0f, 0.0f};
    }
    // 801E2778.
    Vector cross(const Vector& a, const Vector& b) {
        return {a[1] * b[2] - b[1] * a[2], -(a[0] * b[2] - b[0] * a[2]), -(a[1] * b[0] - b[1] * a[0])};
    }
    // 80095EFC.
    Vector clamp(double limit, const Vector& v) {
        double squared = lengthSquared(v);
        if (squared <= static_cast<double>(static_cast<float>(limit * limit))) return v;
        if (0.0 < squared) squared = root(squared);
        return scale(v, static_cast<double>(static_cast<float>(limit / squared)));
    }

    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }
    std::uint32_t bits(float value) {
        std::uint32_t word;
        std::memcpy(&word, &value, sizeof word);
        return word;
    }
    std::uint32_t life(std::int16_t value) { return static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)) << 16; }

    // 800957F8 + 801E1CCC + 801E2938: the beam-space coordinates of a vector relative to a horizontal direction.
    Vector beamSpace(const Vector& direction, const Vector& v) {
        Vector z = direction;
        const double size = length(z);
        if (1.0E-4f <= size) z = scale(z, static_cast<double>(static_cast<float>(1.0 / size)));
        else z = {0.0f, 0.0f, 0.0f};
        Vector up{0.0f, 1.0f, 0.0f};
        if (distanceBetween(up, z) <= 1.0E-4f) up = {0.0f, 0.0f, 1.0f};
        const Vector x = normalize(cross(up, z));
        const Vector y = normalize(cross(z, x));
        // The rows of the transposed rotation are the axes; the translation column is zero.
        return {x[0] * v[0] + x[1] * v[1] + x[2] * v[2] + 0.0f, y[0] * v[0] + y[1] * v[1] + y[2] * v[2] + 0.0f,
            z[0] * v[0] + z[1] * v[1] + z[2] * v[2] + 0.0f};
    }
}

vacuumcapture::Controller::Controller(playermodels::VacuumEntries& entries, Services services) :
    manager_(entries), services_(std::move(services)) {
    if (!services_.jointPosition || !services_.attachmentJoint || !services_.typeStrength || !services_.typeAngle ||
        !services_.typeKind || !services_.typeGroup || !services_.tick || !services_.reducedScale || !services_.sound || !services_.drainSound ||
        !services_.particle || !services_.dust || !services_.burst || !services_.ownerEvent || !services_.setVelocity ||
        !services_.pointParticles || !services_.effectMotion)
        throw std::runtime_error("Original vacuum entries require their sound, particle, owner and model services.");
}

int vacuumcapture::Controller::sourceIndex(const actors::MoveActor* actor) const {
    for (unsigned i = 0; i < 6; ++i) if (manager_.fieldA38[i] == actor) return static_cast<int>(i);
    return -1;
}

int vacuumcapture::Controller::targetIndex(const actors::MoveActor* actor) const {
    for (unsigned i = 0; i < 6; ++i) if (manager_.fieldA50[i] == actor) return static_cast<int>(i);
    return -1;
}

bool vacuumcapture::Controller::containsSource(const actors::MoveActor* actor) const {
    for (const auto& entry : manager_.entries) if (entry->state && entry->source == actor) return true;
    return false;
}

void vacuumcapture::Controller::stopQuiet(unsigned index) {
    slot(index).effect->state = 0; // 80089A00
}

void vacuumcapture::Controller::stopWithPoints(unsigned index) {
    auto& effect = *slot(index).effect;
    effect.stop(services_.pointParticles); // 8008997C
}

bool vacuumcapture::Controller::add(actors::MoveActor& source, actors::MoveActor& target, int mode) {
    // 80082854: a pair is refused when any active entry uses the target as a source, the source as a target, or the
    // same pair.
    for (const auto& entry : manager_.entries) {
        if (!entry->state) continue;
        if (entry->source == &target || entry->target == &source) return false;
        if (entry->source == &source && entry->target == &target) return false;
    }
    for (unsigned i = 0; i < 6; ++i) if (initialize(i, source, target, mode)) return true;
    return false;
}

bool vacuumcapture::Controller::initialize(unsigned index, actors::MoveActor& source, actors::MoveActor& target, int mode) {
    auto& v = slot(index);
    if (v.state) return false;
    auto& e = entries_[index];
    e = Entry{};
    v.state = 1;
    e.mode = mode;
    v.field8 = 0;
    v.trackingChanged = v.blendChanged = 0;
    v.tracking.reset();
    v.blend.reset();
    v.source = &source;
    v.target = &target;
    e.fieldD8 = static_cast<float>(sourceWord2);
    e.fieldDC = (mode == 1 && services_.typeKind(target.type()) == 2) ? static_cast<float>(resetWindow) : 0.0f;
    playermodels::VacuumEffectOwner::Input input;
    input.origin = source.position;
    input.angle = static_cast<std::uint16_t>(source.rotation[1]);
    input.room = source.fieldB4;
    input.target = services_.jointPosition(target, 0);
    v.effect->start(input, services_.particle);
    publish(index);
    e.flags |= 0x200;
    e.state = 6;
    return true;
}

void vacuumcapture::Controller::publish(unsigned index) {
    // 8008177C.
    auto& v = slot(index);
    auto& e = entries_[index];
    e.sourceLife = static_cast<std::int16_t>(v.source->life);
    e.sourceWord1 = static_cast<std::int32_t>(e.fieldD8);
    e.sourceWord2 = sourceWord2;
    e.sourceWord3 = e.fieldC4;
    e.sourceModel = v.effect->state != 0;
    e.sourceWord5 = e.beam[0];
    e.sourceWord6 = -e.beam[2];
    e.sourceWord7 = 1.0f;
    e.flags = 0;
    e.targetLife = static_cast<std::int16_t>(v.target->life);
    e.state = 0;
    e.targetCountWord = e.maximum;
}

void vacuumcapture::Controller::geometry(unsigned index) {
    // 80081834.
    auto& v = slot(index);
    auto& e = entries_[index];
    e.sourcePosition = services_.jointPosition(*v.source, 0);
    e.sourceVelocity = v.source->velocity;
    e.targetPosition = services_.jointPosition(*v.target, services_.attachmentJoint(*v.target));
    e.targetVelocity = v.target->velocity;
    e.distance = static_cast<float>(distanceBetween(e.sourcePosition, e.targetPosition));
    const Vector level{e.targetPosition[0], e.sourcePosition[1], e.targetPosition[2]};
    e.horizontalDistance = static_cast<float>(distanceBetween(e.sourcePosition, level));
    e.direction = subtract(e.targetPosition, e.sourcePosition);
    e.horizontal = e.direction;
    e.direction = normalize(e.direction);
    e.horizontal[1] = 0.0f;
    e.horizontal = normalize(e.horizontal);
    e.reverse = scale(e.direction, -1.0);
    e.reverseHorizontal = scale(e.horizontal, -1.0);
    e.beam = beamSpace(e.horizontal, v.tracking.field18);
    e.beam = scale(e.beam, static_cast<double>(v.tracking.field24));
    const double angle = services_.typeAngle(v.target->type());
    const auto units = static_cast<std::uint32_t>(static_cast<std::int32_t>(
        (65536.0f * static_cast<float>(0.5 * angle)) / 360.0f));
    e.outsideCone = e.beam[2] < -services_.trigonometry.cosineAngle(units);
    e.fieldC4 = v.tracking.field28;
    publish(index);
}

void vacuumcapture::Controller::fade(Entry& e) {
    // 80081A40.
    if (0.0f < e.fieldDC) {
        e.fieldDC -= 1.0f;
        if (e.fieldDC <= 0.0f) {
            e.fieldDC = 0.0f;
        } else {
            const float factor = (1.0f / static_cast<float>(resetWindow)) * resetScale * e.fieldDC;
            e.targetVelocity = scale(e.direction, static_cast<double>(factor));
        }
    }
}

void vacuumcapture::Controller::pull(Entry& e, double inner, double blend) {
    // 800826D8.
    const float gap = static_cast<float>(static_cast<double>(e.horizontalDistance) - inner);
    e.sourceVelocity = plus(e.sourceVelocity, scale(e.horizontal, static_cast<double>(static_cast<float>(blend * static_cast<double>(gap)))));
    e.targetVelocity = plus(e.targetVelocity, scale(e.reverseHorizontal,
        static_cast<double>(static_cast<float>(1.0 - blend) * gap)));
    e.sourceVelocity = clamp(maximumSpeed, e.sourceVelocity);
    e.targetVelocity = clamp(maximumSpeed, e.targetVelocity);
}

void vacuumcapture::Controller::approach(unsigned index) {
    // 80081AEC.
    auto& e = entries_[index];
    if (e.releasing) return;
    if (static_cast<double>(e.horizontalDistance) <= static_cast<double>(farRange)) {
        if (static_cast<double>(e.distance) < static_cast<double>(nearRange)) pull(e, nearRange, 0.5);
    } else {
        float blend = 0.75f;
        if (e.outsideCone) blend = e.strength;
        pull(e, farRange, blend);
    }
}

void vacuumcapture::Controller::shape(Entry& e) {
    // 80081B5C.
    if (0 < e.targetLife) {
        const double along = dot(e.reverseHorizontal, e.sourceVelocity);
        if (0.0 < along) {
            const Vector push = scale(e.horizontal, along * static_cast<double>(globalFactor));
            e.sourceVelocity = plus(e.sourceVelocity, push);
        }
    }
}

void vacuumcapture::Controller::far(unsigned index) {
    // 80082180.
    auto& e = entries_[index];
    if (nearRange < e.horizontalDistance && e.outsideCone) pull(e, e.horizontalDistance + releaseOffset, e.strength);
}

void vacuumcapture::Controller::capture(unsigned index) {
    // 80081BCC.
    auto& v = slot(index);
    auto& e = entries_[index];
    bool assist = false;
    const bool reduced = services_.reducedScale();
    const auto tick = static_cast<std::uint32_t>(static_cast<std::int32_t>(services_.tick()));
    if (!v.tracking.field30) {
        if (0 < e.targetLife && !e.caught) {
            bool drainNow = false;
            if (!e.outsideCone) {
                const double along = dot(e.horizontal, e.sourceVelocity);
                if (static_cast<double>(v.tracking.field2C) < along) {
                    if (coneAssist && 0.0f == e.fieldDC && dot(v.tracking.field18, e.horizontal) <= 0.0) {
                        assist = true;
                        e.flags |= 8;
                    }
                    e.flags |= 4;
                    e.sourceWord7 = static_cast<float>(along / static_cast<double>(v.tracking.field2C));
                }
                e.fieldD0 = 0.0f;
            } else {
                e.flags |= 1;
                e.state = 1;
                const bool shaking = v.target->animationState ==
                    static_cast<std::uint32_t>(services_.catalog.findAnimation(v.target->actorType, "BIRIBIRI"));
                if (shaking) {
                    e.fieldD0 = 0.0f;
                } else {
                    if (!reduced) {
                        if (e.fieldD0 < reducedWindow) {
                            drainNow = (tick & 1u) == 0;
                            e.flags |= 2;
                        } else {
                            drainNow = tick % 10u == 0;
                        }
                    } else if (e.fieldD0 < reducedWindow) {
                        drainNow = tick % reducedModulus == 0;
                        e.flags |= 2;
                    } else {
                        drainNow = tick % 10u == 0;
                    }
                    e.fieldD0 += 1.0f;
                }
            }
            if (!drainNow) {
                e.fieldCC = 0.0f;
            } else {
                float remaining = static_cast<float>(static_cast<std::int32_t>(v.target->life) - 1);
                if (remaining < 0.0f) remaining = 0.0f;
                e.targetLife = static_cast<std::int16_t>(static_cast<std::int32_t>(remaining));
                e.fieldCC += drainStep;
                services_.drainSound();
                if (e.targetLife < 0) e.targetLife = 0;
            }
            services_.sound(0x1033, e.sourcePosition);
            e.fieldC8 = e.fieldCC + (e.beam[2] - v.blend.field0);
        }
    } else if ((e.previousTrackingScale < 0.4f && 0.4f <= v.tracking.field24) || !e.previousTrackingFlag) {
        if (e.beam[0] < 0.5f) {
            if (e.beam[0] < -0.5f) e.flags |= 0x1000;
        } else {
            e.flags |= 0x2000;
        }
    }
    if (assist) {
        if (!e.fieldE8) {
            e.fieldE8 = 1;
            e.fieldE4 += 1;
        }
    } else {
        e.fieldE8 = 0;
    }
    if (e.drain && (e.targetLife < 1 || e.mode == 0)) {
        e.fieldE0 += 1.0f;
        e.flags |= 0x40;
        if (e.fieldE0 <= releaseFrames) {
            pull(e, farRange, 0.0);
        } else {
            e.flags |= 0x10;
            e.state = 2;
            e.releasing = true;
        }
    }
    if (!e.releasing) {
        if (-1.2f <= e.fieldC8) {
            e.fieldD4 = 0.0f;
        } else {
            e.fieldD4 += 1.0f;
            if (e.fieldD4 < static_cast<float>(shortWindow)) {
                services_.sound(0x1035, e.targetPosition);
            } else {
                e.caught = true;
                services_.sound(0x804, e.sourcePosition);
                stopWithPoints(index);
            }
        }
    }
    if (0 < e.targetLife && (v.blend.field4 == 2 || caughtLimit <= e.fieldE4)) {
        e.caught = true;
        services_.sound(0x804, e.sourcePosition);
        stopWithPoints(index);
    }
    if (e.caught) {
        e.flags |= 0x800;
        e.state = 3;
    }
}

void vacuumcapture::Controller::release(unsigned index) {
    // 800821D8.
    auto& v = slot(index);
    auto& e = entries_[index];
    e.flags |= 0x50;
    e.state = 2;
    if (!e.fieldB) {
        if (e.mode == 1) {
            e.flags |= 0x20;
            e.fieldB = true;
        } else {
            services_.sound(0x3a, e.targetPosition);
        }
    } else if (v.tracking.field38) {
        characterevent::Event event(4);
        event.fields[4] = releaseEventKind;
        services_.ownerEvent(*v.target, event);
        services_.sound(0x804, e.sourcePosition);
        services_.dust(e.targetPosition, 0x1e);
        services_.burst(services_.typeGroup(v.target->type()) < 3 ? 0 : 1, v.tracking.field0, v.tracking.fieldC);
        v.state = 0;
        stopQuiet(index);
    }
}

void vacuumcapture::Controller::idle(unsigned index) {
    // 80082300.
    auto& v = slot(index);
    auto& e = entries_[index];
    if (!v.trackingChanged) {
        if (!v.blendChanged) {
            v.state = 0;
            stopQuiet(index);
        } else if (!e.started) {
            e.state = 6;
            e.flags |= 0x140;
        } else if (!v.blend.field8) {
            e.state = 5;
        } else {
            v.state = 0;
            stopWithPoints(index);
        }
    } else if (!e.started) {
        e.flags |= 0x240;
    } else if (!v.tracking.field39) {
        e.flags |= 0x80;
    } else {
        v.state = 0;
        stopQuiet(index);
    }
}

void vacuumcapture::Controller::publishEffect(unsigned index) {
    // 800823E0.
    auto& v = slot(index);
    auto& e = entries_[index];
    if (v.state) {
        playermodels::VacuumEffectOwner::Input input;
        input.origin = v.tracking.field0;
        input.direction = v.tracking.fieldC;
        input.angle = static_cast<std::uint16_t>(v.source->rotation[1]);
        input.room = v.source->fieldB4;
        input.target = e.targetPosition;
        const auto& model = *v.effect->model;
        if (v.effect->state && model.count) input.targetDirection = model.points[model.count - 1].previous;
        if ((e.flags & 0x4000) && v.effect->state && model.count)
            throw std::runtime_error("Original vacuum release trail (800823E0 flag 0x4000) is not ported.");
        input.strength = e.fieldC8;
        input.mode = 0.0f < e.fieldD4 ? 1 : 0;
        input.alternate = (e.flags & 0x4000) ? 1 : 0;
        input.distance = beamDistance;
        v.effect->start(input, services_.particle);
    }
    if (v.effect->state) services_.effectMotion(*v.effect); // 80089A0C
    if (v.state && (e.flags & 0x4000) && v.effect->state && v.effect->model->count) {
        const auto& model = *v.effect->model;
        e.targetVelocity = subtract(model.points[model.count - 1].position, e.targetPosition);
    }
}

void vacuumcapture::Controller::updateEntry(unsigned index, float strength, std::int32_t sourceCount,
    std::int32_t targetCount, std::uint32_t maximum, bool flag) {
    // 800814E4.
    auto& v = slot(index);
    auto& e = entries_[index];
    if (v.state) {
        e.sourceCount = sourceCount;
        e.targetCount = targetCount;
        const float raised = static_cast<float>(static_cast<double>(strength) +
            static_cast<double>(0.2f * static_cast<float>(sourceCount - 1)));
        e.strength = raised < 1.0f ? raised : 1.0f;
        e.drain = flag;
        e.maximum = maximum;
        e.targetCountWord = maximum;
        geometry(index);
        if (!v.trackingChanged || !v.blendChanged) {
            idle(index);
        } else {
            if (!e.started) {
                if (e.mode == 1) services_.dust(e.targetPosition, 0x1e);
                e.started = true;
                v.field8 = 1;
            }
            fade(e);
            if (!v.tracking.field31) {
                approach(index);
                shape(e);
            }
            if (!e.releasing) {
                if (!v.tracking.field31) {
                    capture(index);
                    far(index);
                } else {
                    e.flags |= 0x4000;
                }
            } else {
                release(index);
            }
            e.previousTrackingScale = v.tracking.field24;
            e.previousTrackingFlag = v.tracking.field30 != 0;
        }        v.trackingChanged = v.blendChanged = 0;
    }
    publishEffect(index);
}

void vacuumcapture::Controller::rebuildLists() {
    // 80083108.
    auto& m = manager_;
    m.fieldC90 = m.fieldC94 = 0;
    m.fieldA38.fill(nullptr);
    m.fieldA50.fill(nullptr);
    m.fieldBB8.fill(0);
    m.fieldBD0.fill(0);
    m.fieldBE8.fill(0.0f);
    for (unsigned i = 0; i < 6; ++i) {
        const auto& v = *m.entries[i];
        actors::MoveActor* source = v.state ? v.source : nullptr;
        actors::MoveActor* target = v.state ? v.target : nullptr;
        if (source && sourceIndex(source) < 0) m.fieldA38[m.fieldC90++] = source;
        if (target && targetIndex(target) < 0) m.fieldA50[m.fieldC94++] = target;
    }
    for (unsigned k = 0; k < 6; ++k) {
        if (m.fieldA38[k]) {
            unsigned count = 0;
            for (unsigned j = 0; j < 6; ++j) {
                const auto& v = *m.entries[j];
                if ((v.state ? v.source : nullptr) == m.fieldA38[k]) targetsOfSource[k][count++] = v.state ? v.target : nullptr;
            }
            m.fieldBB8[k] = count;
            targetsOfSource[k][count] = nullptr;
        }
        if (m.fieldA50[k]) {
            unsigned count = 0;
            for (unsigned j = 0; j < 6; ++j) {
                const auto& v = *m.entries[j];
                if ((v.state ? v.target : nullptr) == m.fieldA50[k]) sourcesOfTarget[k][count++] = v.state ? v.source : nullptr;
            }
            m.fieldBD0[k] = count;
            sourcesOfTarget[k][count] = nullptr;
        }
    }
    for (unsigned k = 0; k < 6; ++k) {
        double best = 0.0;
        if (m.fieldA38[k]) {
            for (unsigned j = 0; j < m.fieldBB8[k]; ++j) {
                double candidate = best;
                if (targetsOfSource[k][j]) {
                    candidate = services_.typeStrength(targetsOfSource[k][j]->type());
                    if (candidate < best) candidate = best;
                }
                best = candidate;
            }
        }
        m.fieldBE8[k] = static_cast<float>(best);
    }
}

void vacuumcapture::Controller::aggregate() {
    // 80083534.
    auto& m = manager_;
    std::array<unsigned, 6> sourceCounts{}, targetCounts{};
    for (unsigned i = 0; i < 6; ++i) {
        const auto& v = *m.entries[i];
        const auto& e = entries_[i];
        actors::MoveActor* source = v.state ? v.source : nullptr;
        actors::MoveActor* target = v.state ? v.target : nullptr;
        if (source) {
            const auto k = static_cast<unsigned>(sourceIndex(source));
            auto& record = m.sourceRecords[k];
            if (!sourceCounts[k]) {
                record = {life(e.sourceLife), static_cast<std::uint32_t>(e.sourceWord1), static_cast<std::uint32_t>(e.sourceWord2),
                    bits(e.sourceWord3), e.sourceModel ? 1u : 0u, bits(e.sourceWord5), bits(e.sourceWord6), bits(e.sourceWord7),
                    bits(e.sourceWord8), e.flags, 0u, i + 1u};
                sourceVelocity[k] = e.sourceVelocity;
            } else {
                if (static_cast<std::int16_t>(e.sourceLife) < static_cast<std::int16_t>(record[0] >> 16)) record[0] = life(e.sourceLife);
                if (number(record[3]) < e.sourceWord3) record[3] = bits(e.sourceWord3);
                if (e.sourceWord1 < static_cast<std::int32_t>(record[1])) {
                    record[1] = static_cast<std::uint32_t>(e.sourceWord1);
                    record[2] = static_cast<std::uint32_t>(e.sourceWord2);
                }
                record[9] |= e.flags;
                sourceVelocity[k] = plus(sourceVelocity[k], e.sourceVelocity);
            }
            ++sourceCounts[k];
        }
        if (target) {
            const auto k = static_cast<unsigned>(targetIndex(target));
            auto& record = m.targetRecords[k];
            if (!targetCounts[k]) {
                record = {life(e.targetLife), i + 1u, i + 1u, e.targetCountWord, e.state, 0u, i + 1u};
                targetVelocity[k] = e.targetVelocity;
            } else {
                targetVelocity[k] = plus(targetVelocity[k], e.targetVelocity);
            }
            ++targetCounts[k];
        }
    }
    for (unsigned k = 0; k < 6; ++k) {
        if (m.fieldA38[k]) {
            m.sourceRecords[k][10] = k + 1u;
            sourceVelocity[k] = scale(sourceVelocity[k], static_cast<double>(static_cast<float>(1.0 / static_cast<double>(static_cast<float>(sourceCounts[k])))));
        }
        if (m.fieldA50[k]) {
            m.targetRecords[k][5] = k + 1u;
            targetVelocity[k] = scale(targetVelocity[k], static_cast<double>(static_cast<float>(1.0 / static_cast<double>(static_cast<float>(targetCounts[k])))));
        }
    }
    for (unsigned k = 0; k < 6; ++k) m.sourceRecords[k][8] = bits(m.fieldA38[k] ? m.fieldBE8[k] : 0.0f);
    for (unsigned k = 0; k < 6; ++k) {
        sourceTargets[k] = playeractor::Target{};
        if (!m.fieldA38[k]) continue;
        const auto& record = m.sourceRecords[k];
        sourceTargets[k].model = record[4] ? m.entries[record[11] - 1]->effect->model.get() : nullptr;
        sourceTargets[k].flags = record[9];
        sourceTargets[k].actor = targetsOfSource[k].data();
        sourceTargets[k].horizontal = number(record[5]);
        sourceTargets[k].vertical = number(record[6]);
    }
}

playeractor::Target* vacuumcapture::Controller::captureTarget(const actors::MoveActor& source) {
    const int slot = sourceIndex(&source);
    return slot < 0 ? nullptr : &sourceTargets[slot];
}

void vacuumcapture::Controller::applyVelocities() {
    // 800839C0.
    for (unsigned k = 0; k < 6; ++k) {
        if (manager_.fieldA38[k]) services_.setVelocity(*manager_.fieldA38[k], sourceVelocity[k]);
        if (manager_.fieldA50[k]) services_.setVelocity(*manager_.fieldA50[k], targetVelocity[k]);
    }
}

const actors::MoveActor* vacuumcapture::Controller::firstSource(const std::array<std::uint32_t, 7>& record) const {
    if (!record[5]) return nullptr;
    return sourcesOfTarget.at(record[5] - 1)[0];
}

void vacuumcapture::Controller::update() {
    // 800829FC.
    unsigned active = 0;
    for (const auto& v : manager_.entries) if (v->state) ++active;
    if (!active || manager_.fieldC98 < active) manager_.fieldC98 = active;
    for (unsigned i = 0; i < 6; ++i) {
        const auto& v = *manager_.entries[i];
        const actors::MoveActor* source = v.state ? v.source : nullptr;
        const actors::MoveActor* target = v.state ? v.target : nullptr;
        const int sourceSlot = sourceIndex(source);
        const int targetSlot = targetIndex(target);
        float strength = 0.0f;
        std::int32_t sourceCount = 0, targetCount = 0;
        if (source) {
            // A source that was not aggregated yet reads the neighbouring words of the original arrays (index -1).
            strength = sourceSlot >= 0 ? manager_.fieldBE8[sourceSlot] : number(manager_.fieldBD0[5]);
            sourceCount = sourceSlot >= 0 ? static_cast<std::int32_t>(manager_.fieldBB8[sourceSlot]) : 0;
        }
        if (target) targetCount = static_cast<std::int32_t>(targetSlot >= 0 ? manager_.fieldBD0[targetSlot] : manager_.fieldBB8[5]);
        updateEntry(i, strength, sourceCount, targetCount, manager_.fieldC98, true);
    }
    rebuildLists();
    aggregate();
    applyVelocities();
}
