#pragma once

#include "moneyscatterowner.h"
#include "billscatterscene.h"
#include "leafscatterscene.h"

namespace moneyscatter {
    void updateFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
        const Vector& cameraPosition, const Vector& cameraDirection);
}
