#include "game/effects/particlescenecontext.h"

#include <cmath>
#include <stdexcept>

void particles::SceneContext::refreshPlayer(double heightOffset, const playeractor::View& player,
    const actormodel::InitialPose& pose, enemies::Manager& manager, const roomtraversal::State& visibility) {
    if (!player.position || !player.controls || !player.field8A4 || !player.field8A8) {
        throw std::runtime_error("Original particle scene context requires a populated Player view.");
    }
    actorPosition = player.position;
    angle = static_cast<float>(static_cast<std::uint16_t>(player.angle));
    orientation = pose.horizontalDirection(static_cast<std::uint16_t>(player.angle));
    magnitude = player.magnitude;
    suction.strength = player.field8B0;
    control = player.controls->field2C;
    suction.position = player.field8A4;
    suction.direction = player.field8A8;
    height = static_cast<float>(double((*actorPosition)[1]) + heightOffset);
    field38 = player.field840;
    field3C = player.field844;
    field48 = player.fieldF24;
    field4C = player.field84C;
    room = manager.playerRoom(0);
    for (unsigned i = 0; i < 3; ++i) endpoint[i] = (*suction.direction)[i] * distance;
    for (unsigned i = 0; i < 3; ++i) endpoint[i] = (*suction.position)[i] + endpoint[i];
    field54 = 0.25f + 0.2f * suction.strength;
    field5C = player.fieldF17;
    visible = (visibility.record(room, 0).flags & 1) != 0;
    field59 = player.field8C9;
}

void particles::refreshSecondary(SceneContext& context, double heightOffset, const SecondarySource* source) {
    if (!source || !source->active) {
        context.field48 = 0;
        return;
    }
    if (!source->position) throw std::runtime_error("Original secondary particle context has no actor position.");
    context.field48 = source->field14;
    context.actorPosition = source->position;
    const auto& v = source->velocity;
    context.magnitude = std::sqrt(std::fma(v[2], v[2], v[0] * v[0]) + v[1] * v[1]);
    context.orientation = source->direction;
    context.height = static_cast<float>(double((*source->position)[1]) + heightOffset);
}

void particles::SceneContexts::refresh(const playeractor::View& view, const actormodel::InitialPose& pose,
    enemies::Manager& manager, const roomtraversal::State& visibility, const SecondarySource* source) {
    player.refreshPlayer(200, view, pose, manager, visibility);
    refreshSecondary(secondary, 150, source);
}
