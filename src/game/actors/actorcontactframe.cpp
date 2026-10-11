#include "game/actors/moveactor.h"
#include "game/events/runtimeflags.h"

#include <cmath>

void actors::MoveActor::updateContactFrame(const Registry& registry, enemies::Manager& manager,
    const actorcollision::Manager& collision, const playerresources::RecordPool44& pool,
    const mapcollision::State& map, const actorcollision::Catalog& catalog, gameflags::State& globalFlags) {
    if (flags & 0x60) return;
    if (!parent) {
        for (unsigned i = 0; i < 3; ++i) position[i] += velocity[i];
        const float squared = (velocity[2] * velocity[2] + velocity[0] * velocity[0]) + velocity[1] * velocity[1];
        speed = squared <= 0.00001f ? 0 : std::sqrt((velocity[0] * velocity[0] + velocity[1] * velocity[1]) + velocity[2] * velocity[2]);
    }
    if (!(flags & 0x80)) {
        updateActorContacts(registry, manager, collision, pool);
        updateMapContact(map, catalog);
    }
    if (construction.catalog.type(actorType).animations.at(animationState)[5] & 0x100) return;
    auto probe = position;
    probe[1] += 40 * matrix[5];
    const auto room = construction.rooms.resolve(probe, fieldB4);
    flags &= ~0x100000u;
    if (room == 0xffffffff) {
        flags |= 0x100000;
        position = previousPosition;
    } else {
        if (!actorId && room != fieldB4) for (unsigned i = 0x55; i < 0xaa; ++i) globalFlags.setOff(i);
        fieldB4 = room;
    }
}
