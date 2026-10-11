#pragma once

#include "game/actors/actorcollisionowner.h"
#include "game/actors/actorcollisionquery.h"

namespace actorcollision {
    bool intersect(Vector& displacement, const Volume& first, const Vector& firstPosition,
        const Volume& second, const Vector& secondPosition);
}
