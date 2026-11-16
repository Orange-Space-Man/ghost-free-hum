#pragma once

#include "game/enemies/enemycapturemotion.h"
#include "game/actors/actorcollisionowner.h"
#include "game/actors/actorinitialpose.h"
#include "game/actors/moveactor.h"
#include "game/world/roomlocation.h"
#include "game/world/mapcollisionstate.h"

namespace ghostcapture {
    class Collision {
    public:
        Collision(const actorcollision::Catalog& shapes, const actormodel::InitialPose& pose,
            const roomlocation::Rooms& rooms, const mapcollision::State& map);
        void reset();
        void update(actors::MoveActor& actor, double padding);
        unsigned room = 0xffffffff;
        Vector minimum{}, maximum{}, center{}, size{}, boundaryNormal{}, polygonNormal{};
        unsigned contact = 0;
        bool changed = false, polygonContact = false;

    private:
        const actorcollision::Catalog& shapes;
        const actormodel::InitialPose& pose;
        const roomlocation::Rooms& rooms;
        const mapcollision::State& map;
        Vector sphereCenter(const actors::MoveActor& actor, const actorcollision::Record& record) const;
        void contain(actors::MoveActor& actor) const;
    };
}
