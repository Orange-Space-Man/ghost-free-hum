#pragma once

#include "game/actors/actormodelinstance.h"

namespace actormodel {
    void blendTransform(double weight, const Matrix& current, const Matrix& previous, Matrix& output);
    void blendPose(Instance& instance);
    void updateEnvelopes(Instance& instance);
}
