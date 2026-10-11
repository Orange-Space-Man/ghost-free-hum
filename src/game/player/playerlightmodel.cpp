#include "game/player/playerlightmodel.h"

#include <algorithm>

playermodels::LightModel::LightModel(const SharedModels& shared, gameplaycamera::CameraOwner& camera) : camera(&camera) {
    instance = std::make_unique<j3dmodel::ModelInstance>(shared.model(SharedModel::Light), 0x10000);
    unsigned count = 0;
    for (auto& material : instance->model->materials.entries) {
        for (auto& matrix : material->texture->matrices) {
            if (count == 3) break;
            if (!matrix || matrix->info[1] != 3) continue;
            materials[count] = material.get();
            matrices[count] = matrix.get();
            std::copy_n(matrix->info.begin() + 0x24, 64, originalMatrices[count].begin());
            ++count;
        }
        if (count == 3) break;
    }
}
