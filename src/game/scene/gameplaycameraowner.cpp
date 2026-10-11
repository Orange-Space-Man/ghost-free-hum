#include "game/scene/gameplaycameraowner.h"

gameplaycamera::CameraOwner::CameraOwner() : fieldOfView(26), aspect(4.0f / 3.0f) {
    reset();
}

void gameplaycamera::CameraOwner::reset() {
    callback = 0;
    flags = 0;
    target = {0, 0, 0};
    fieldA4 = 0;
    fieldA8 = 0;
    fieldCC = fieldCE = 0;
    position = {0, 200, 1800};
    up = {0, 1, 0};
    viewport = {0, 0, 640, 480};
    nearPlane = 16;
    farPlane = 8192;
    matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    fieldE0 = 0;
}

gameplaycamera::Owners::Owners() {
    for (auto& camera : cameras) camera = std::make_unique<CameraOwner>();
    active = cameras[0].get();
}

gameplaycamera::Owners::~Owners() {
    for (auto& camera : cameras) camera.reset();
}
