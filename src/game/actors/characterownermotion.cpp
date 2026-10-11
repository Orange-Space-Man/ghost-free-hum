#include "game/actors/characterownermotion.h"
#include "game/actors/actorpathsampler.h"

#include <cmath>
#include <stdexcept>

void characterowner::updateMotion(Owner& owner, const actormodel::Catalog& models, const actormodel::Resources& resources,
    const actormodel::InitialPose& pose, const std::array<float, 3>* originalScratch) {
    auto& state = owner.state();
    if (!state.field8d4) return;
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    std::array<float, 3> velocity;
    if (state.field8d4 == 1) {
        for (unsigned axis = 0; axis < 3; ++axis) velocity[axis] = state.field8d8[axis] - actor.position[axis];
        const float square = std::fma(velocity[2], velocity[2], velocity[0] * velocity[0]) + velocity[1] * velocity[1];
        const float distance = std::sqrt(square);
        if (!(distance >= 0x1p-23f)) {
            state.field8d4 = 0;
            return;
        }
        const float estimate = 1.0f / std::sqrt(square);
        const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
        for (auto& component : velocity) component *= inverse;
        float speed = state.field8e4;
        if (!(distance >= speed)) {
            state.field8d4 = 0;
            speed = distance;
        }
        for (auto& component : velocity) component *= speed;
    } else if (state.field8d4 == 2) {
        const auto& type = models.type(actor.actorType);
        const auto& animation = type.animations.at(actor.animationState);
        const bool reverse = (animation[5] & 2) != 0;
        const float previous = actor.animationFrame - actor.animationFrameStep;
        const bool needsScratch = (!reverse && !(previous <= actor.animationFrame)) ||
            !resources.path(static_cast<std::uint16_t>(type.model), static_cast<std::uint16_t>(animation[0]));
        if (needsScratch && !originalScratch) {
            throw std::runtime_error("Original owner path motion reads unwritten stack data; captured scratch values are required.");
        }
        if (originalScratch) velocity = *originalScratch;
        if (actoranimation::pathDisplacement(resources, type.model, animation[0], previous, actor.animationFrame, reverse, velocity)) {
            const auto matrix = pose.rotation({0, static_cast<std::int16_t>(state.field8e8), 0});
            const auto unrotated = velocity;
            for (unsigned row = 0; row < 3; ++row) {
                const auto offset = row * 4;
                velocity[row] = std::fma(matrix[offset + 2], unrotated[2], matrix[offset] * unrotated[0]) +
                    (matrix[offset + 3] + matrix[offset + 1] * unrotated[1]);
            }
        } else state.field8d4 = 0;
    } else {
        if (!originalScratch) throw std::runtime_error("Original owner motion mode reads unwritten stack data.");
        velocity = *originalScratch;
    }
    actor.setVelocity(velocity);
}
