#include "game/actors/moveactor.h"
#include "game/actors/actorcollisionvolume.h"

namespace {
    actorcollision::Volume volume(const actors::MoveActor& actor, const actorcollision::Manager& collision) {
        if (actor.field788) return *actor.field788;
        const auto& element = *collision.elements.at(actor.actorType);
        return {element.field8, element.secondary, element.extended};
    }
}

bool actors::MoveActor::resolveActorContact(MoveActor& other, const actorcollision::Manager& collision) {
    actorcollision::Vector displacement{};
    if (!actorcollision::intersect(displacement, volume(*this, collision), position, volume(other, collision), other.position)) return false;
    if (construction.catalog.type(actorType).field4 == 5) {
        for (unsigned i = 0; i < 3; ++i) other.position[i] -= displacement[i];
        other.field5C = other.position;
    } else for (unsigned i = 0; i < 3; ++i) position[i] += displacement[i];
    field9C = static_cast<std::int32_t>(other.actorId);
    other.field9C = static_cast<std::int32_t>(actorId);
    const auto support = [&](MoveActor& supported, const MoveActor& supporting) {
        supported.fieldA0 = static_cast<std::int32_t>(supporting.actorId);
        const auto& animation = construction.catalog.type(supported.actorType).animations.at(supported.animationState);
        if (animation[5] & 0x20) {
            supported.selectAnimation(animation[4]);
            supported.velocity[1] = 0;
        }
    };
    if (displacement[1] > 0 && velocity[1] < 0) support(*this, other);
    else if (displacement[1] < 0 && other.velocity[1] < 0) support(other, *this);
    return true;
}
