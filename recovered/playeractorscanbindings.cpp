#include "playeractorscanbindings.h"

#include <cstring>
#include <stdexcept>

playerscanner::ActorBindings::ActorBindings(enemies::Manager& manager, const actors::Registry& actors,
    const actormodel::Catalog& models, const actorcollision::Catalog& collisions, const roomtraversal::State& visibility) :
    manager(manager), actors(actors), models(models), collisions(collisions), visibility(visibility) {}

playerscanner::Hit playerscanner::ActorBindings::query(const Vector& position, const Vector& direction) const {
    std::vector<ActorTarget> targets;
    for (const auto& slot : manager.slots()) {
        if (slot.state != 2) continue;
        auto* actor = dynamic_cast<actors::MoveActor*>(&slot.owner->actor());
        if (!actor || actor->actorType != 0x3e || !actor->parent) continue;
        const auto& parent = *actor->parent;
        if (models.type(parent.actorType).field14 != 3) continue;
        const unsigned room = parent.fieldB4 >> 24;
        if (!visibility.marked(room, 0, 0) && !visibility.marked(room, 1, 0)) continue;
        const auto& records = collisions.type(actor->actorType).records;
        if (records.empty()) throw std::runtime_error("Original scanner proxy requires its first collision record.");
        float radius;
        std::memcpy(&radius, &records.front()[1], sizeof(radius));
        targets.push_back({actor->position, radius, static_cast<int>(parent.actorId), true, true, true, true, true});
    }
    return actorTarget(targets, position, direction, 1000000.0f);
}

unsigned playerscanner::ActorBindings::type(unsigned actor) const {
    const auto* value = actors.actor(actor);
    if (!value) throw std::runtime_error("Scanner result references an absent actor.");
    return value->actorType;
}

std::optional<unsigned> playerscanner::ActorBindings::message(unsigned actor) const {
    const auto* value = actors.actor(actor);
    if (!value) throw std::runtime_error("Scanner message references an absent actor.");
    const auto* owner = manager.activeOwner(*value);
    if (!owner || !owner->source.table || owner->source.row < 0) return std::nullopt;
    std::uint32_t message;
    if (!owner->source.table->getValue(owner->source.row, "msg_no", message)) return std::nullopt;
    return message;
}
