#include "game/enemies/enemyitemstrategy.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/enemies/enemystrategycatalog.h"
#include "game/actors/actorcollisionvolume.h"
#include "game/player/playerattractionevent.h"

#include <cmath>
#include <stdexcept>

unsigned enemystrategy::itemCondition(unsigned type) {
    switch (type) {
    case 1: return 0x7b;
    case 3: return 0x80;
    case 4: return 0x35;
    case 6: return 0x7c;
    case 7: return 0x7d;
    case 8: return 0x7e;
    case 9: return 0x81;
    case 10: return 0x82;
    case 11: return 0x84;
    case 12: return 0x86;
    case 13: return 0x8e;
    case 14: return 0x90;
    case 15: return 0x91;
    case 16: return 0x96;
    case 17: return 0xa9;
    case 18: return 0xb3;
    case 19: return 0xb4;
    default: return 0x7f;
    }
}

enemystrategy::FiniteItem::FiniteItem(ItemEnvironment& environment, const Factory& factory) :
    physics(environment.collision, environment.surfaces, environment.suction.pose), environment(environment), factory(factory),
    suctionEnvironment{environment.suction.rooms, environment.suction.visibility, environment.suction.pose,
        environment.suction.context, environment.suction.force, environment.suction.effects, environment.suction.flags, *this},
    suction(suctionEnvironment, environment.retainedRemaining) {
    if (!environment.unwrittenDirection) throw std::runtime_error("Original suction scratch storage is not bound.");
    suction.countdown = 0;
    suction.emitEffects = 0;
}

enemystrategy::Item::Item(ItemEnvironment& environment, const Factory& factory) : FiniteItem(environment, factory) {
    parameters.add("mFlagOnId", 1, 0);
}

actors::MoveActor& enemystrategy::FiniteItem::actor() const {
    if (!owner) throw std::runtime_error("Original EnItem has no character owner.");
    return dynamic_cast<actors::MoveActor&>(owner->actor());
}

playeractor::Player& enemystrategy::FiniteItem::player(unsigned index) const {
    auto* result = environment.collection.actors.actor(environment.collection.enemies.appearPoint(index).actor);
    if (!result) throw std::runtime_error("Original EnItem player is not constructed.");
    return dynamic_cast<playeractor::Player&>(*result);
}

void enemystrategy::FiniteItem::bind(Strategy& strategy) {
    strategy.owner = owner;
    strategy.context = context;
}

void enemystrategy::Item::initializeItem() {
    const auto path = "/param/th/EnItem_" + environment.types.name(actor().type()) + ".prm";
    const int entry = environment.common.findPathResource(path);
    if (entry >= 0) {
        std::string error;
        if (!parameters.load(environment.common.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
    }
    flags |= 6;
    if (!owner->source.table || owner->source.row < 0) flags |= 0x10;
}

void enemystrategy::FiniteItem::initializeBehavior() {
    switch (state) {
    case 0: initializeItem(); break;
    case 1:
        if (!condition) throw std::runtime_error("Original EnItem condition is absent.");
        condition->setNextState(0);
        break;
    case 2:
        if (!appearance) throw std::runtime_error("Original EnItem appearance is absent.");
        appearance->setNextState(0);
        break;
    case 3: beginActive(); break;
    }
}

void enemystrategy::FiniteItem::behavior() {
    switch (state) {
    case 0:
        if (flags & 4) setNextState(3);
        else {
            std::uint32_t conditionType = 2, appearanceType = 3;
            if (owner->source.table && owner->source.row >= 0) {
                owner->source.table->getValue(owner->source.row, "cond_type", conditionType);
                owner->source.table->getValue(owner->source.row, "appear_type", appearanceType);
            }
            condition.reset();
            condition = factory.construct(environment.strategies.constructor(itemCondition(conditionType)));
            bind(*condition);
            appearance = appearanceType == 4 ? static_cast<Strategy*>(&fade) : &immediate;
            bind(*appearance);
            setNextState(1);
        }
        break;
    case 1:
        if (!condition) throw std::runtime_error("Original EnItem condition is absent.");
        condition->update();
        if (condition->state == 0x100) setNextState(2);
        else actor().setVelocity({});
        break;
    case 2:
        if (!appearance) throw std::runtime_error("Original EnItem appearance is absent.");
        appearance->update();
        if (appearance->state == 0x100) setNextState(3);
        break;
    case 3:
        if (!collected) {
            updateActive();
            if (touchingPlayer()) collectItem();
        } else actor().setVelocity({});
        break;
    }
}

void enemystrategy::Item::beginActive() {
    auto& object = actor();
    environment.sounds.play(0x1875, object.position, 0);
    beginFinite();
    initialRoom = object.fieldB4;
}

void enemystrategy::FiniteItem::beginFinite() {
    auto& object = actor();
    bind(physics);
    physics.setNextState(0);
    groundedFrames = 0;
    suction.initialize(object, 0, environment.durationBase);
    lifetime.initialize();
}

bool enemystrategy::FiniteItem::canAttract() const {
    for (unsigned i = 0; i < environment.collection.enemies.playerCount(); ++i) if (player(i).view.field8C9) return false;
    return lifetime.parameters.entries()[0].bits < frames && (actor().fieldE0 & 255) != 0;
}

void enemystrategy::Item::updateActive() {
    physics.update();
    if (physics.state == 2) {
        if (std::fabs(actor().velocity[1]) > 3) environment.sounds.play(0x18b3, actor().position, 0);
        ++groundedFrames;
    }
    updateFinite();
    if (((initialRoom >> 16) & 255) != ((actor().fieldB4 >> 16) & 255)) owner->event(10);
}

void enemystrategy::FiniteItem::updateFinite(bool infinite) {
    if (canAttract()) {
        auto& object = actor();
        suction.update(object, 0, 25, environment.unwrittenDirection());
        object.scale = {1, 1, 1};
        object.rotation[0] = object.rotation[1] = 0;
        object.previousRotation[1] = object.previousRotation[2] = 0;
    }
    lifetime.update(*owner, infinite, collected, flags, expired);
}

bool enemystrategy::FiniteItem::touchingPlayer() const {
    const auto& object = actor();
    if ((object.fieldE0 & 255) != 255 || frames < 30) return false;
    const auto& view = player().view;
    if (!view.volume || !view.position) throw std::runtime_error("Original player collision view is not bound.");
    actorcollision::Vector displacement{};
    if (object.field788) return actorcollision::intersect(displacement, *object.field788, object.position, *view.volume, *view.position);
    return actorcollision::intersect(displacement, environment.volumes.type(object.type()), object.position, *view.volume, *view.position);
}

bool enemystrategy::FiniteItem::collectItem() {
    auto& object = actor();
    if (object.parent || expired || collected) return false;
    playerevents::ItemRequest request;
    request.animated = (flags & 2) != 0;
    request.actor = static_cast<std::int32_t>(object.id());
    prepareCollection(request);
    if (!playerevents::acceptItem(player(), request, environment.collection)) return false;
    collected = true;
    flags &= ~0x10u;
    onCollected();
    auto& recipient = player();
    playerevents::registerCapture(*recipient.components->bits[0], *recipient.components->bits[4],
        environment.feedback.properties, object.type(), recipient.life);
    if (!request.animated) owner->event(4);
    return true;
}

std::uint32_t enemystrategy::FiniteItem::captureEnd(characterevent::Event&) {
    if (flags & 1) environment.suction.effects.create(0x46, actor().position, {});
    return 1;
}

void enemystrategy::FiniteItem::prepareCollection(playerevents::ItemRequest&) {}

std::uint32_t enemystrategy::FiniteItem::playerLeftRoom(characterevent::Event&) {
    const bool remove = (flags & 0x10) != 0;
    if (remove) {
        flags &= ~1u;
        owner->event(4);
    }
    return remove;
}

void enemystrategy::FiniteItem::attraction(unsigned id) {
    playerevents::beginAttraction(player(), environment.collection.actors, nullptr, static_cast<std::int32_t>(id), 10);
}

void enemystrategy::FiniteItem::completion(const itemsuction::Completion& payload) {
    playerevents::completeSuction(player(), payload, environment.feedback);
}

void enemystrategy::FiniteItem::collect(actors::MoveActor& object) {
    if (&object != &actor()) throw std::runtime_error("Original item suction callback changed actor ownership.");
    collectItem();
}

void enemystrategy::Item::onCollected() {
    const auto flag = static_cast<std::uint8_t>(parameters.entries()[0].bits);
    if (flag) environment.suction.flags.setOn(flag);
    environment.suction.effects.create(0x46, actor().position, {});
}
