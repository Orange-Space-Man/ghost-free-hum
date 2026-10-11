#include "game/actors/characterowner.h"
#include "game/actors/actorregistry.h"
#include "game/world/dooranimationpose.h"
#include "game/actors/charactergenerator.h"

#include <stdexcept>

void characterowner::Owner::updateFlagTimer() {
    const auto flags = dynamic_cast<actors::MoveActor&>(actor()).flags;
    if (flags & 0x40000) {
        values.field8d0 |= 1;
        return;
    }
    if (!(flags & 0x20000) || !values.field8cc) return;
    if (--values.field8cc == 0) values.field8d0 |= 1;
}

void characterowner::Owner::releaseFromManager() {
    if (values.generator) values.generator->remove(*this);
    manager().deactivate(actor().id());
}

void characterowner::Owner::updateReleaseTimer() {
    if (!values.field91c) return;
    --values.field920;
    if (static_cast<std::int32_t>(values.field920) < 0) {
        releaseFromManager();
        values.field91c = false;
    }
}

void characterowner::Owner::updateEventTimers() {
    for (const unsigned index : {0u, 1u, 2u, 4u, 3u}) {
        auto& timer = values.fields908[index];
        if (static_cast<std::int32_t>(timer) > 0) --timer;
    }
}

void characterowner::Owner::updateAnimationEffect() {
    if (!values.field894) return;
    const auto& actorState = dynamic_cast<actors::MoveActor&>(actor());
    const auto matrix = actorState.model ? dooranimation::concatenate(actorState.matrix, actorState.model->matrix(0, 0)) : actorState.matrix;
    values.field894->setMatrix(matrix);
    if (values.field898 == actorState.animationState) {
        values.field894->setVisible(true);
        values.field894->setRunning(actorState.animationSpeed != 0);
    } else {
        values.field894->release();
        values.field894 = nullptr;
        values.field898 = 0xffffffff;
    }
}

void characterowner::Owner::releaseActorReference(void*& user) {
    if (!user) return;
    user = nullptr;
    values.actorReferences = static_cast<std::int32_t>(static_cast<std::uint32_t>(values.actorReferences) - 1);
    if (values.actorReferences < 1) {
        auto* child = static_cast<actors::MoveActor*>(owned.dependentActor);
        if (!child) throw std::runtime_error("Original dependent actor is null during reference release.");
        dynamic_cast<actors::MoveActor&>(actor()).detachChild(*child);
        manager().deactivate(child->actorId);
        owned.dependentActor = nullptr;
    }
}

void characterowner::Owner::updateDependentActors(const actors::Registry& registry) {
    const auto& actorState = dynamic_cast<actors::MoveActor&>(actor());
    if (values.field890 != -1 && static_cast<std::uint32_t>(values.field890) != actorState.animationState && values.actorUsers[1]) {
        releaseActorReference(values.actorUsers[1]);
        values.field890 = -1;
    }
    auto* child = static_cast<actors::MoveActor*>(values.field928);
    if (!child) return;
    if (actorState.flags & 0xc000) child->flags |= 0xc000;
    else child->flags &= 0xffff3fff;
    const auto* player = dynamic_cast<playeractor::Player*>(registry.actor(manager().appearPoint(0).actor));
    if (!player) throw std::runtime_error("Original dependent actor update requires player zero.");
    if (!player->view.field895) releaseActorReference(values.field928);
}

void characterowner::Owner::updateSpecialAttachment(const actors::Registry& registry, const enemycreation::Catalog& creation) {
    const auto* player = dynamic_cast<playeractor::Player*>(registry.actor(manager().appearPoint(0).actor));
    if (!player) throw std::runtime_error("Original special attachment requires player zero.");
    if (!player->view.field895 || values.field928) return;
    auto& actorState = dynamic_cast<actors::MoveActor&>(actor());
    if ((actorState.flags & 0xc000) || creation.group(actorState.actorType) != 3 ||
        !manager().roomVisible(actorState.resolvePositionRoom())) return;
    values.actorReferences = static_cast<std::int32_t>(static_cast<std::uint32_t>(values.actorReferences) + 1);
    if (values.actorReferences < 2) {
        const auto id = manager().createActor(0x3e, 0);
        if (id == 0xffffffff) throw std::runtime_error("Original special attachment exhausted the actor pool.");
        auto* child = registry.actor(id);
        if (!child) throw std::runtime_error("Original special attachment actor is unavailable.");
        owned.dependentActor = child;
        actorState.attachChild(*child, strategies.attachmentJoint(actorState.actorType), nullptr, 0);
    }
    auto* child = static_cast<actors::MoveActor*>(owned.dependentActor);
    if (!child) throw std::runtime_error("Original special attachment retained a null actor.");
    values.field928 = child;
    child->fieldE4 = 0xffffffff;
    child->selectAnimation("SINPAKU");
}
