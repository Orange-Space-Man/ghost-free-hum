#pragma once

#include "game/effects/particleforces.h"

namespace particles {
    unsigned advance(Emitter& emitter, const Forces& forces, std::uint32_t& randomState);
}
