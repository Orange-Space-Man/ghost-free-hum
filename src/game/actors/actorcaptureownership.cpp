#include "game/actors/moveactor.h"

#include <stdexcept>

void actors::MoveActor::retainCapture(const std::array<std::uint8_t, 0x430>& retained) {
    if (capture) throw std::runtime_error("Original actor capture storage is already retained.");
    capture = std::make_unique<actorcapture::State>(retained);
}

const actorcapture::State* actors::MoveActor::captureState() const { return capture.get(); }
actorcapture::State* actors::MoveActor::captureState() { return capture.get(); }

void actors::MoveActor::initializeCapture(actorcapture::Motion& motion, const std::array<float, 3>& target, std::uint16_t type) {
    if (flags & 0x80000) return;
    field100 = target;
    if (!model->model) return;
    if (!capture) throw std::runtime_error("Original actor capture storage has not been retained.");
    actorcapture::initializeActor(motion, *capture, *this, target, type);
}

void actors::MoveActor::updateCapture(actorcapture::Motion& motion, const playermodels::VacuumPath* path,
    const actorcapture::Vector* suctionDirection, actormodel::Matrix& retainedTransform,
    const actorcapture::Vector* originalScratch, const float* originalRegister, const bool* emptyCompletion) {
    if (!(flags & 0x80000) || !model->model) return;
    if (!capture) throw std::runtime_error("Active actor capture has no retained storage.");
    actorcapture::updateActor(motion, *capture, *this, path, suctionDirection, retainedTransform,
        originalScratch, originalRegister, emptyCompletion);
}
