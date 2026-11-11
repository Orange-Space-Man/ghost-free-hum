#pragma once

#include "game/world/doorresources.h"
#include "game/models/modeldrawtraversal.h"
#include "game/models/s3dmodel.h"

namespace modelshadow {
    s3dmodel::Batch prepare(const doors::ModelResource& resource, const modeldraw::DrawCommand& command,
        const dooranimation::Matrix& matrix);
}
