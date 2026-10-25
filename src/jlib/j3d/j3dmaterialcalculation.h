#pragma once

#include "jlib/j3d/j3dmodelcalculation.h"

namespace j3dmodel {
    void calculateTexture(TextureMatrix& texture, const TransformTables& tables);
    void calculateMaterial(Material& material, const Matrix& joint, const Matrix& camera,
        std::uint32_t systemFlags, const TransformTables& tables);
}
