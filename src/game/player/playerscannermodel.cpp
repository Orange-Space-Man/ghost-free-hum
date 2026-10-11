#include "game/player/playerscannermodel.h"

playermodels::ScannerModels::ScannerModels(const SharedModels& shared, gameplaycamera::CameraOwner& camera) : camera(&camera) {
    scanner = std::make_unique<j3dmodel::ModelInstance>(shared.model(SharedModel::Scanner), 0x10000);
    warp = std::make_unique<j3dmodel::ModelInstance>(shared.model(SharedModel::Warp), 0x10000);
}
