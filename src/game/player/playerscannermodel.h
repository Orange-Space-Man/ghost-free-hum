#pragma once

#include "game/scene/gameplaycameraowner.h"
#include "game/player/playersharedmodels.h"

namespace playermodels {
    class ScannerModels {
    public:
        ScannerModels(const SharedModels& shared, gameplaycamera::CameraOwner& camera);
        std::array<std::uint8_t, 3> state{};
        float field4 = 0;
        std::uint32_t field8 = 0xffffffff, fieldC = 0xffffffff;
        float field10 = 0;
        gameplaycamera::CameraOwner* camera;
        std::unique_ptr<j3dmodel::ModelInstance> scanner, warp;
    };
}
