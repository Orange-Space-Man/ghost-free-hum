#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/effects/sharedmodeleffect.h"
#include "game/world/mapcollisionstate.h"

characterownership::Resource* characterowner::Owner::acquireShared(modeleffects::Factory& effects,
    const enemytypes::Manager& parameters, const mapcollision::State& collision) {
    owned.sharedCount = static_cast<std::int32_t>(static_cast<std::uint32_t>(owned.sharedCount) + 1);
    if (owned.sharedCount < 2) {
        const auto& actorState = dynamic_cast<actors::MoveActor&>(actor());
        auto position = actorState.position;
        collision.projectFloor(position);
        owned.sharedResource = modeleffects::createShared(effects, parameters, actorState.type(), position);
    }
    return owned.sharedResource;
}

void characterowner::Owner::updateProjection(modeleffects::Factory& effects, const enemytypes::Manager& parameters,
    const mapcollision::State& collision) {
    const auto& actorState = dynamic_cast<actors::MoveActor&>(actor());
    if ((actorState.fieldE0 & 255) < 129) {
        if (owned.sharedUsers[2]) owned.releaseShared(owned.sharedUsers[2]);
    } else if ((parameters.parameters(actorState.type()).entries().at(20).bits & 1) && !values.flags904[0] && !owned.sharedUsers[2]) {
        owned.sharedUsers[2] = acquireShared(effects, parameters, collision);
    }
    auto* resource = owned.sharedResource;
    if (!resource) return;
    auto position = actorState.position;
    if (collision.projectFloor(position)) {
        resource->setVisible(true);
        resource->setAttachmentPosition(position, {});
    } else {
        resource->setVisible(false);
    }
    resource->setVisible((actorState.fieldE0 & 255) >= 129);
}
