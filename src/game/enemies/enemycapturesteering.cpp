#include "game/enemies/enemycapturesteering.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
    ghostcapture::Vector normalize(ghostcapture::Vector value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length >= 0.0001f) {
            const float scale = static_cast<float>(1.0 / double(length));
            for (auto& component : value) component *= scale;
        } else value.fill(0);
        return value;
    }
}

ghostcapture::Steering::Steering(std::function<std::uint32_t()> value) : random(std::move(value)) {
    if (!random) throw std::runtime_error("Original capture steering requires the shared random source.");
}

float ghostcapture::Steering::fraction() { return 2.3283064365386963e-10f * static_cast<float>(random()); }

void ghostcapture::Steering::begin(Motion& motion, const SteeringInput& input) {
    motion.damping = 0.97f;
    motion.maximumSpeed = 40;
    motion.force = {};
    motion.forceMagnitude = 4;
    probability = 0;
    disabled = false;
    state = 0;
    initialize(motion, input);
}

ghostcapture::Vector ghostcapture::Steering::direction(const Motion& motion, const SteeringInput& input) {
    const float minor = 6.984919587171845e-11f * static_cast<float>(random());
    const float x = input.position[0] - input.anchor[0], z = input.position[2] - input.anchor[2];
    const float sourceX = motion.source[0] - input.anchor[0], sourceZ = motion.source[2] - input.anchor[2];
    const bool positiveX = x >= 0, positiveZ = z >= 0;
    bool alongX;
    const auto choose = [&] { return fraction() < 0.5f; };
    if (positiveX) {
        if (positiveZ) {
            if (sourceX < 0 || sourceZ >= 0) {
                if (sourceX >= 0 || sourceZ < 0) alongX = choose();
                else alongX = false;
            } else alongX = true;
        } else if (sourceX < 0 || sourceZ < 0) {
            if (sourceX >= 0 || sourceZ >= 0) alongX = choose();
            else alongX = false;
        } else alongX = true;
    } else if (positiveZ) {
        if (sourceX >= 0 || sourceZ >= 0) {
            if (sourceX < 0 || sourceZ < 0) alongX = choose();
            else alongX = false;
        } else alongX = true;
    } else if (sourceX >= 0 || sourceZ < 0) {
        if (sourceX < 0 || sourceZ >= 0) alongX = choose();
        else alongX = false;
    } else alongX = true;
    Vector result{(positiveX ? -1.0f : 1.0f) * (alongX ? 1.0f : minor), 0,
        (positiveZ ? -1.0f : 1.0f) * (alongX ? minor : 1.0f)};
    const auto& normal = input.contactNormal;
    const auto& toward = motion.horizontalTowardSource;
    const float dot = (toward[0] * normal[0] + toward[1] * normal[1]) + toward[2] * normal[2];
    for (unsigned i = 0; i < 3; ++i) result[i] -= normal[i] + dot * normal[i];
    return normalize(result);
}

void ghostcapture::Steering::initialize(Motion& motion, const SteeringInput& input) {
    if (state == 2) { state = 0; initialize(motion, input); return; }
    if (state == 0) {
        random();
        frames = 7.0f + fraction() * 8.0f;
        const float sign = fraction() < 0.5f ? -1.0f : 1.0f;
        const float radians = 0.01745329238474369f * (sign * (42.5f + fraction() * 42.5f));
        const float sine = std::sin(radians), cosine = std::cos(radians);
        const auto& toward = motion.towardSource;
        const Vector rotated{-(cosine * toward[0] + sine * toward[2]), 0,
            -(-sine * toward[0] + cosine * toward[2])};
        const auto unit = normalize(rotated);
        for (unsigned i = 0; i < 3; ++i) motion.force[i] = 4.0f * unit[i];
    } else if (state == 1) {
        frames = 20.0f + fraction() * 10.0f;
        const auto unit = direction(motion, input);
        for (unsigned i = 0; i < 3; ++i) impulse[i] = 4.0f * unit[i];
        motion.force = impulse;
    }
}

void ghostcapture::Steering::update(Motion& motion, const SteeringInput& input) {
    if (disabled) { disabled = false; return; }
    if (state == 2) { state = 0; initialize(motion, input); return; }
    if (state > 2) return;
    if (state == 1) {
        const float scale = -0.4f + fraction() * 0.8f;
        for (unsigned i = 0; i < 3; ++i) motion.force[i] = impulse[i] + scale * impulse[i];
    }
    if (frames > 0) frames = input.interrupted ? 0 : frames - 1;
    if (!(frames <= 0)) return;
    if (state == 0) {
        if (attempts > 5 || input.contact) {
            state = 1;
            initialize(motion, input);
            attempts = 0;
        } else {
            if (probability <= fraction()) attempts += 1;
            else state = 2;
            initialize(motion, input);
        }
    } else {
        if (fraction() >= 0.5f) state = 1;
        else state = probability <= fraction() ? 0 : 2;
        initialize(motion, input);
    }
}
