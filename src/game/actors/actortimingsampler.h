#pragma once

#include "game/actors/actormodelresources.h"

namespace actoranimation {
    float sampleTiming(const actormodel::Resource* resource, std::string_view name, double frame);
    float timingCrossing(const actormodel::Resource* resource, std::string_view name, double previous, double current);
    std::uint8_t timingAlpha(float value);
}
