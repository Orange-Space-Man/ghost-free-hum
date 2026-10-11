#include "game/actors/characterownervacuum.h"
#include "game/actors/moveactor.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>

void characterowner::updateVacuum(Owner& owner, playermodels::VacuumEntries& entries, modeleffects::Factory& effects,
    const enemytypes::Manager& parameters, const mapcollision::State& collision,
    const actormodel::Catalog& models, enemies::Runtime& runtime) {
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    auto& state = owner.state();
    auto& owned = owner.ownership();
    if (!entries.containsTarget(&actor)) {
        state.captureRecord = nullptr;
        if (owned.sharedUsers[1]) owned.releaseShared(owned.sharedUsers[1]);
        return;
    }
    auto* record = entries.targetRecord(&actor);
    if (!state.captureRecord && record && (*record)[4] != 3 && (*record)[4] != 5) {
        if (!owned.sharedUsers[1]) owned.sharedUsers[1] = owner.acquireShared(effects, parameters, collision);
        owner.event(9);
    }
    state.captureRecord = record;
    if (!record) return;
    if (actor.animationState != static_cast<std::uint32_t>(models.findAnimation(actor.actorType, "BIRIBIRI"))) {
        actor.life = static_cast<std::uint16_t>((*record)[0] >> 16);
    }
    if ((*record)[4] == 2 && !state.flags904[0]) {
        characterevent::Event event(3);
        event.fields[0] = actor.actorType == 0xde || actor.actorType == 0xdd ? 1 : runtime.random() & 1;
        owner.event(event);
        if (owned.sharedUsers[1]) owned.releaseShared(owned.sharedUsers[1]);
        if (owned.sharedUsers[2]) owned.releaseShared(owned.sharedUsers[2]);
    }
}

void characterowner::updateSuction(Owner& owner, playermodels::VacuumEntries& entries, const actors::Registry& actors,
    const enemycreation::Catalog& creation,
    const std::function<void(unsigned, const std::array<float, 3>&, unsigned)>& sound) {
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    auto& state = owner.state();
    if (!entries.containsTarget(&actor)) return;
    const auto* record = entries.targetRecord(&actor);
    if (!record) return;
    playermodels::VacuumTrackingBlend blend;
    if ((*record)[4] == 3 || (*record)[4] == 5) {
        state.flags904[1] = false;
        return;
    }
    if (state.flags904[1]) {
        state.flags904[1] = false;
        blend.field4 = 2;
        state.captureRecord = nullptr;
    } else if (state.flags904[0]) {
        if (!(actor.flags & 0x80000)) {
            actor.flags |= 0xc400;
            for (auto& child : actor.children) if (child.actor) child.actor->flags |= 0xc400;
            blend.field4 = 1;
        } else {
            const int index = owner.manager().nearestPlayer(actor.position);
            if (index < 0) throw std::runtime_error("Original captured actor has no nearby Player.");
            const auto* player = dynamic_cast<const playeractor::Player*>(actors.actor(owner.manager().appearPoint(index).actor));
            if (!player || !player->view.field8A4) throw std::runtime_error("Original Player capture target is unavailable.");
            actor.field100 = *player->view.field8A4;
            actor.setVelocity({0, 0, 0});
            if (creation.group(actor.actorType) == 2) {
                if (!sound) throw std::runtime_error("Original capture sound delivery is unbound.");
                sound(0x408f, actor.position, 0);
            }
        }
    }
    entries.setBlend(&actor, blend);
}
