#pragma once

#include "game/scene/gameplayspawn.h"
#include "game/models/s3denvironment.h"

namespace gameplaycamera {
    bool initialize(const std::vector<std::uint8_t>& executable, const gameplayspawn::Spawn& spawn,
        s3denvironment::Camera& camera, std::string& error);
}
