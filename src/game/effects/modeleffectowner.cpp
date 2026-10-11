#include "game/effects/modeleffectowner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

modeleffects::Owner::Owner(const std::array<unsigned, 3>& counts) {
    for (unsigned i = 0; i < frames.size(); ++i) frames[i].resize(counts[i]);
    reset();
}

void modeleffects::Owner::reset() {
    for (auto& values : frames) std::fill(values.begin(), values.end(), 0);
    elapsed = 0;
    frame = 0;
    flags = 6;
}

void modeleffects::Owner::release() {
    reset();
    flags |= 8;
}

int modeleffects::Owner::attachmentState() { return (flags & 1) == 0; }

void modeleffects::Owner::setAttachmentPosition(const std::array<float, 3>& position, const std::array<float, 3>&) {
    setPosition(position);
}

void modeleffects::Owner::setVisible(bool visible) {
    if (visible) flags |= 4;
    else flags &= 0xfb;
}

void modeleffects::Owner::setMatrix(const j3dmodel::Matrix& matrix) {
    if ((flags & 2) && !(flags & 0x10)) {
        if (!model) throw std::runtime_error("Original model effect has no model instance.");
        model->matrix = matrix;
    }
}

void modeleffects::Owner::setRunning(bool running) {
    if (running) flags |= 2;
    else flags &= 0xfd;
}

void modeleffects::Owner::setPosition(const std::array<float, 3>& position) {
    if (!model) throw std::runtime_error("Original model effect has no model instance.");
    model->matrix = {1, 0, 0, position[0], 0, 1, 0, position[1], 0, 0, 1, position[2]};
}

void modeleffects::Owner::setScale(const std::array<float, 3>& scale) {
    if (!model) throw std::runtime_error("Original model effect has no model instance.");
    model->scale = scale;
}

void modeleffects::Owner::setFrame(double value) {
    const auto truncated = std::trunc(value);
    frame = !std::isfinite(truncated) || truncated < std::numeric_limits<std::int32_t>::min() ||
        truncated > std::numeric_limits<std::int32_t>::max() ? std::numeric_limits<std::int32_t>::min() :
        static_cast<std::int32_t>(truncated);
    for (auto& values : frames) std::fill(values.begin(), values.end(), frame);
}
