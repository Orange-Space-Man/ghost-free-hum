#pragma once

#include "game/actors/actormodelresources.h"

namespace actoranimation {
    float sampleFloatTrack(const std::vector<std::uint8_t>& bytes, unsigned descriptor, unsigned values, double frame);
    void samplePath(const actormodel::Resource& path, double frame, std::array<float, 3>& position,
        std::array<std::int16_t, 3>* rotation = nullptr);
    bool pathDisplacement(const actormodel::Resources& resources, unsigned model, unsigned key, double previous,
        double current, bool reverse, std::array<float, 3>& displacement);
}
