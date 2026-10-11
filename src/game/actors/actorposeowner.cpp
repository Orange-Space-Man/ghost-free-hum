#include "game/actors/moveactor.h"
#include "game/actors/actorposeupdate.h"

void actors::MoveActor::samplePose(const actormodel::Matrix* root) {
    matrix = construction.pose.jointMatrix({scale, {rotation[0], rotation[1], rotation[2]}, position}, true);
    if (flags & 0x60) return;
    if (!model->model) blendFramesRemaining = 0;
    const auto& entry = construction.resources.entry(construction.catalog.type(actorType).model);
    model->model = entry.archive ? entry.archive->model : nullptr;
    model->bindKey(entry.archive ? entry.archive->keys.at(keyIndex) : nullptr);
    model->frame = animationFrame;
    model->blend = float(blendFramesRemaining) / float(blendFramesTotal);
    actormodel::samplePose(*model, construction.pose, root);
}
