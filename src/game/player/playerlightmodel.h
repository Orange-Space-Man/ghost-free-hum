#pragma once

#include "game/scene/gameplaycameraowner.h"
#include "game/player/playersharedmodels.h"
#include "jlib/j3d/j3dmodelcalculation.h"

namespace playeractor { class Player; }

namespace playermodels {
    class LightModel {
    public:
        LightModel(const SharedModels& shared, gameplaycamera::CameraOwner& camera);
        void prepare(const j3dmodel::Matrix& view, float projectionDepth,
            const j3dmodel::TransformTables& tables, SharedModels& shared);
        std::uint8_t state = 0;
        playeractor::Player* owner = nullptr;
        gameplaycamera::CameraOwner* camera;
        std::array<float, 3> fieldC{}, field18{}, field24{0, 0, 1}, field30{0, 0, 1};
        float field3C = 0, field40 = 0;
        std::array<float, 2> field44;
        float field4C = 0, field50 = 0;
        std::unique_ptr<j3dmodel::ModelInstance> instance;
        std::array<j3dmodel::Material*, 3> materials{};
        std::array<j3dmodel::TextureMatrix*, 3> matrices;
        std::array<std::array<std::uint8_t, 64>, 3> originalMatrices;
    };
}
