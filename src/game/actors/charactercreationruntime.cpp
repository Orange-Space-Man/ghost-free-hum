#include "game/actors/charactercreationruntime.h"
#include "game/actors/characterownerdamage.h"
#include "game/effects/particlescene.h"
#include "game/player/playermanager.h"

#include <stdexcept>

characterowner::CreationRuntime::CreationRuntime(characterownership::Resources& resources,
    const charactercreation::Properties& properties, playerrecord::Record& player, roomlighting::Pool& lights,
    jaudio::PositionOwners& sounds) : effects(resources), properties(properties), player(player), lights(lights), sounds(sounds) {}

void characterowner::CreationRuntime::bind(enemies::Manager& value, const enemystrategy::Factory& strategies) {
    for (auto& links : actorLinks) links.reset();
    linkedActors.fill(nullptr);
    manager = &value;
    factory = &strategies;
}

characterownership::Resources& characterowner::CreationRuntime::resources() { return effects; }

void characterowner::CreationRuntime::bindCapture(CaptureServices* services) { capture = services; }
void characterowner::CreationRuntime::bindItemCreation(characteritems::Creation* value) { items = value; }

void characterowner::CreationRuntime::bindUpdates(UpdateServices* services) { updates = services; }

void characterowner::CreationRuntime::update(Owner& owner) {
    if (!manager || &owner.manager() != manager || !updates || !updates->players.entries) {
        throw std::runtime_error("Original character-owner update services are not bound to EnManager.");
    }
    auto& services = *updates;
    auto& entries = *services.players.entries;
    owner.updateFlagTimer();
    if (owner.state().field8d4) {
        updateMotion(owner, services.models, services.resources, services.pose, services.originalMotionScratch);
    }
    updateDamage(owner, properties, services.models, player, [&](unsigned type, const std::array<float, 3>& position) {
        services.particles.createDamage(type, position);
    });
    updateVacuum(owner, entries, services.modelEffects, services.parameters, services.collision, services.models, services.runtime);
    owner.updateDependentActors(services.actors);
    owner.ownership().updateAttachments(dynamic_cast<actors::MoveActor&>(owner.actor()));
    owner.updateProjection(services.modelEffects, services.parameters, services.collision);
    owner.updateAnimationEffect();
    owner.updateStrategy();
    updateSuction(owner, entries, services.actors, services.creation, services.sound);
    owner.updateReleaseTimer();
    owner.updateEventTimers();
    owner.updateSpecialAttachment(services.actors, services.creation);
}

std::uint32_t characterowner::CreationRuntime::event(Owner& owner, characterevent::Event& event) {
    const auto group = event.message == 26 ? properties.group(owner.actor().type()) : 0;
    return dispatchEvent(owner, event, group, capture, items);
}

characterownership::ActorLinks& characterowner::CreationRuntime::links(enemies::Actor& actor) {
    if (!manager) throw std::runtime_error("Original character creation runtime has no EnManager.");
    const auto index = actor.id();
    auto& links = actorLinks.at(index);
    if (!links || linkedActors.at(index) != &actor) {
        links = std::make_unique<characterownership::Links>(dynamic_cast<actors::MoveActor&>(actor), *manager, sounds);
        linkedActors[index] = &actor;
    }
    return *links;
}

void characterowner::CreationRuntime::resetActor(enemies::Actor& actor, const jmp::ToolDataRef& source) {
    dynamic_cast<actors::MoveActor&>(actor).resetOwner(source);
}

void characterowner::CreationRuntime::clearInactiveFlags(enemies::Actor& actor) {
    dynamic_cast<actors::MoveActor&>(actor).flags &= 0xffff3bdf;
}

void characterowner::CreationRuntime::initializeActor(enemies::Actor& actor, unsigned type) {
    dynamic_cast<actors::MoveActor&>(actor).initializeType(type);
}

std::unique_ptr<enemystrategy::Strategy> characterowner::CreationRuntime::constructStrategy(std::uint32_t constructor,
    Owner&, unsigned capacity) {
    if (!factory || capacity != 0x800) throw std::runtime_error("Original character strategy storage is not initialized.");
    return factory->construct(constructor);
}

void characterowner::CreationRuntime::updateEffects(Owner& owner) {
    properties.updateEffects(dynamic_cast<actors::MoveActor&>(owner.actor()), lights);
}

void characterowner::CreationRuntime::creationEvent(Owner& owner) {
    properties.initializeLife(dynamic_cast<actors::MoveActor&>(owner.actor()), owner.state(), player);
}

std::uint32_t characterowner::CreationRuntime::color(unsigned type) { return properties.color(type); }

void characterowner::CreationRuntime::writeColor(enemies::Actor& actor, std::uint32_t color) {
    properties.writeColor(dynamic_cast<actors::MoveActor&>(actor), color);
}
