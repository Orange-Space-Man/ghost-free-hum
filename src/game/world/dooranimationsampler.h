#pragma once

#include "game/world/dooranimationresource.h"

namespace dooranimation {
    struct Key {
        float frame = 0;
        float value = 0;
        float incoming = 0;
        float outgoing = 0;
    };

    float interpolate(float frame, const Key& first, const Key& second);
    double clampFrame(const Resource& resource, double frame);
    void updateFrame(const Resource& resource, float& frame);
    float sampleChannel(const Resource& resource, unsigned track, unsigned channel, float frame);
}
