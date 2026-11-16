#include "game/enemies/enemycapturemotion.h"
#include <cmath>

namespace {
    float squared(const ghostcapture::Vector& value) {
        return (value[0] * value[0] + value[1] * value[1]) + value[2] * value[2];
    }

    ghostcapture::Vector normalized(ghostcapture::Vector value) {
        const float length = std::sqrt(squared(value));
        if (length >= 0.0001f) {
            const float scale = static_cast<float>(1.0 / double(length));
            for (auto& component : value) component *= scale;
        } else value.fill(0);
        return value;
    }
}

void ghostcapture::Motion::track(const Vector& position, const Vector* captureSource) {
    source = captureSource ? *captureSource : Vector{};
    for (unsigned i = 0; i < 3; ++i) towardSource[i] = source[i] - position[i];
    horizontalTowardSource = towardSource;
    towardSource = normalized(towardSource);
    horizontalTowardSource[1] = 0;
    horizontalTowardSource = normalized(horizontalTowardSource);
}

bool ghostcapture::Motion::recoil(bool contact, const Vector& normal) {
    bool active = false;
    if (recoilEnabled) {
        active = recoilFrames > 0;
        if (active) {
            recoilFrames -= 1;
            force = recoilForce;
        }
        if (recoilFrames <= 0 && contact) {
            recoilFrames = 30;
            for (unsigned i = 0; i < 3; ++i) recoilForce[i] = -forceMagnitude * normal[i];
        }
    }
    return active;
}

ghostcapture::Vector ghostcapture::Motion::velocity(const Vector& position, const Vector& previous,
    float floor, float typeHeight, const Vector* contactNormal) const {
    Vector result;
    const float retention = 1.0f - (1.0f - damping);
    for (unsigned i = 0; i < 3; ++i) result[i] = (previous[i] + force[i]) * retention;
    if (contactNormal) {
        const auto& normal = *contactNormal;
        const float dot = (normal[0] * result[0] + normal[1] * result[1]) + normal[2] * result[2];
        if (dot < 0) for (unsigned i = 0; i < 3; ++i) result[i] += -dot * normal[i];
    }
    if (!preserveVertical) {
        if (overrideHeight) result[1] = (height + floor) - position[1];
        else if (typeHeight >= 0) result[1] = static_cast<float>(double(typeHeight) + double(floor)) - position[1];
    }
    const float magnitude = squared(result);
    if (!(magnitude <= maximumSpeed * maximumSpeed)) {
        double length = magnitude;
        if (length > 0) {
            double inverse = 1.0 / std::sqrt(length);
            inverse = 0.5 * inverse * (3.0 - length * inverse * inverse);
            inverse = 0.5 * inverse * (3.0 - length * inverse * inverse);
            length = static_cast<float>(length * 0.5 * inverse * (3.0 - length * inverse * inverse));
        }
        const float scale = static_cast<float>(double(maximumSpeed) / length);
        for (auto& component : result) component *= scale;
    }
    return result;
}
