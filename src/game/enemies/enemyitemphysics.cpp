#include "game/enemies/enemyitemphysics.h"
#include "game/actors/moveactor.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    actors::MoveActor& actor(enemystrategy::Strategy& strategy) {
        if (!strategy.owner) throw std::runtime_error("Original item physics has no character owner.");
        return dynamic_cast<actors::MoveActor&>(strategy.owner->actor());
    }

    float number(const gameparameters::Parameters& parameters, unsigned index) {
        const auto bits = parameters.entries().at(index).bits;
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

enemystrategy::ItemPhysics::ItemPhysics(const mapcollision::State& collision, const actors::RotatingSurfaces& surfaces,
    const actormodel::InitialPose& pose) : collision(collision), surfaces(surfaces), pose(pose) {
    parameters.add("sGravity", 4, 0x40400000);
    parameters.add("sResistance", 4, 0x3a6bedfa);
    parameters.add("sFriction", 4, 0x3f19999a);
    parameters.add("sReflect", 4, 0x3f19999a);
    parameters.add("sEpsilon", 4, 0x3f800000);
    parameters.add("sAccelResistance", 4, 0x3f000000);
}

void enemystrategy::ItemPhysics::initializeBehavior() {
    if (state == 0) acceleration = actor(*this).velocity;
}

void enemystrategy::ItemPhysics::behavior() {
    if (state == 0) {
        setNextState(1);
        return;
    }
    if (state != 1 && state != 2) return;
    auto& object = actor(*this);
    const bool grounded = (object.flags & 0x10000) != 0;
    if (state == 2 && !grounded) {
        setNextState(1);
        return;
    }
    const float gravity = number(parameters, 0), resistance = number(parameters, 1);
    const float friction = number(parameters, 2), reflect = number(parameters, 3);
    const float epsilon = number(parameters, 4), decay = number(parameters, 5);
    std::array<float, 3> change;
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float drag = object.velocity[axis] * resistance;
        change[axis] = (axis == 1 ? -gravity : 0.0f) - drag;
        if (state == 1) {
            change[axis] += acceleration[axis];
            acceleration[axis] *= decay;
        }
    }
    if (grounded) {
        const auto impulse = itemphysics::reflection(collision, object.fieldB2, object.velocity, reflect, friction);
        for (unsigned axis = 0; axis < 3; ++axis) change[axis] += impulse[axis];
        if (state == 1) setNextState(2);
    }
    const auto wall = itemphysics::reflection(collision, object.fieldB0, object.velocity, reflect, friction);
    for (unsigned axis = 0; axis < 3; ++axis) change[axis] += wall[axis];
    if (state == 2) {
        std::array<float, 3> surface{};
        playerorientation::Quaternion orientation;
        surfaces.velocity(pose, object.position, surface, orientation);
        for (unsigned axis = 0; axis < 3; ++axis) change[axis] += surface[axis];
    }
    auto velocity = object.velocity;
    for (unsigned axis = 0; axis < 3; ++axis) {
        velocity[axis] += change[axis];
        const float magnitude = velocity[axis] < 0 ? -velocity[axis] : velocity[axis];
        if (!(magnitude >= epsilon)) velocity[axis] = 0;
    }
    object.setVelocity(velocity);
}
