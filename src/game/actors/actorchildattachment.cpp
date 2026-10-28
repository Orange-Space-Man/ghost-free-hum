#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"
#include "game/actors/actorposeblend.h"

#include <cmath>
#include <stdexcept>

namespace {
    float magnitude(const std::array<float, 3>& vector) {
        return std::sqrt((vector[0] * vector[0] + vector[1] * vector[1]) + vector[2] * vector[2]);
    }

    void normalize(std::array<float, 3>& vector) {
        const float length = magnitude(vector);
        if (length >= 0.0001f) {
            const float inverse = 1.0f / length;
            for (auto& value : vector) value *= inverse;
        } else vector.fill(0);
    }
}

bool actors::MoveActor::attachChild(MoveActor& child, unsigned joint, const actormodel::Matrix* transform, std::uint8_t orientation) {
    if (parent) return false;
    unsigned index = 0;
    while (index < children.size() && children[index].actor) ++index;
    if (index == children.size() || child.parent) return false;
    for (const auto& entry : child.children) if (entry.actor) return false;
    child.parent = this;
    auto& entry = children[index];
    entry.actor = &child;
    entry.joint = joint;
    entry.transformed = transform != nullptr;
    if (transform) entry.transform = *transform;
    entry.orientation = orientation;
    return true;
}

bool actors::MoveActor::updateChildAttachment(MoveActor& child, unsigned joint,
    const actormodel::Matrix* transform, std::uint8_t orientation) {
    if (parent) return false;
    for (auto& entry : children) {
        if (entry.actor != &child) continue;
        entry.joint = joint;
        entry.transformed = transform != nullptr;
        if (transform) entry.transform = *transform;
        entry.orientation = orientation;
        return true;
    }
    return false;
}

actormodel::Matrix actors::MoveActor::childMatrix(unsigned index) const {
    const auto& entry = children.at(index);
    if (!entry.actor || !model) throw std::runtime_error("Original child transform requires an attached actor model.");
    auto joint = model->matrix(0, entry.joint);
    if (!entry.orientation) joint = {1, 0, 0, joint[3], 0, 1, 0, joint[7], 0, 0, 1, joint[11]};
    auto result = dooranimation::concatenate(matrix, joint);
    if (entry.transformed) result = dooranimation::concatenate(result, entry.transform);
    return result;
}

void actors::MoveActor::updateChildren() {
    for (const auto& entry : children) if (entry.actor) entry.actor->flags = (entry.actor->flags & ~0x60u) | (flags & 0x60);
    for (unsigned i = 0; i < children.size(); ++i) {
        auto* child = children[i].actor;
        if (!child) continue;
        const auto previous = child->position;
        auto transform = childMatrix(i);
        child->field5C = {transform[3], transform[7], transform[11]};
        child->position = child->field5C;
        child->setVelocity({child->position[0] - previous[0], child->position[1] - previous[1], child->position[2] - previous[2]});
        child->setRotation({0, 0, 0});
        transform[3] = transform[7] = transform[11] = 0;
        if (!(child->flags & 0x60)) {
            child->samplePose(&transform);
            actormodel::blendPose(*child->model);
            actormodel::updateEnvelopes(*child->model);
            child->fieldB4 = fieldB4;
        }
    }
}

bool actors::MoveActor::detachChild(MoveActor& child) {
    if (parent) return false;
    unsigned index = 0;
    while (index < children.size() && children[index].actor != &child) ++index;
    if (index == children.size() || !child.parent) return false;
    child.parent = nullptr;
    auto transform = childMatrix(index);
    std::array<float, 3> lengths;
    for (unsigned column = 0; column < 3; ++column) {
        std::array<float, 3> vector{transform[column], transform[column + 4], transform[column + 8]};
        lengths[column] = magnitude(vector);
        normalize(vector);
        normalize(vector);
        for (unsigned row = 0; row < 3; ++row) transform[column + row * 4] = vector[row];
    }
    transform[3] = transform[7] = transform[11] = 0;
    child.setRotation(construction.pose.angles(transform));
    const float uniformScale = 0.33333334f * ((lengths[2] + lengths[0]) + lengths[1]);
    child.scale.fill(uniformScale);
    children[index].actor = nullptr;
    return true;
}
