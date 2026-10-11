#include "resources/glme01addresses.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/events/missionresources.h"

#include <stdexcept>

characterowner::Owner::Owner(Kind kind, const enemystrategy::Catalog& strategies, Runtime& runtime) :
    ownerKind(kind), strategies(strategies), runtime(runtime) {
    reset();
    if (ownerKind == Kind::Ordinary) {
        pathData = std::make_unique<jmp::ToolData>();
    }
}

characterowner::Owner::~Owner() {
    pathData.reset();
    owned.strategy.reset();
}

void characterowner::Owner::attach(enemies::Actor& actor, enemies::Manager& manager) {
    object = &actor;
    parent = &manager;
}

enemies::Actor& characterowner::Owner::actor() {
    if (!object) {
        throw std::runtime_error("Original character owner has no attached actor.");
    }
    return *object;
}

void characterowner::Owner::reset() {
    values.generator = nullptr;
    values.actorUsers = {};
    values.field928 = nullptr;
    values.actorReferences = 0;
    values.life = 0;
    values.field8a0 = 0;
    values.field890 = -1;
    values.field894 = nullptr;
    values.field898 = 0;
    values.minimum = 0;
    values.maximum = 255;
    values.speed = 1;
    values.target = 1;
    values.controlActor = object;
    values.field8c8 = -1;
    values.field8cc = 0;
    values.field8d0 = false;
    values.field8d4 = 0;
    values.field8e4 = 0;
    values.field8e8 = 0;
    values.field8ea = false;
    values.captureRecord = nullptr;
    values.flags904 = {};
    values.fields908 = {};
    values.field91c = false;
    values.field920 = 0;
    values.field924 = false;
    owned.reset();
    if (object) {
        runtime.resetActor(*object, source);
    }
}

void characterowner::Owner::initializeStrategy(unsigned type) {
    owned.strategy.reset();
    owned.strategy = runtime.constructStrategy(strategies.constructor(type), *this, 0x800);
    if (!owned.strategy) {
        throw std::runtime_error("Original character strategy construction failed.");
    }
    if (ownerKind == Kind::Ordinary) {
        owned.strategy->owner = this;
        values.field954 = 0;
        values.fields93c = {};
        values.field94c = 0;
        values.field950 = 0;
    }
}

bool characterowner::Owner::activate(unsigned type) {
    characterownership::Acquired acquired;
    if (!owned.acquire(type, runtime.resources(), acquired)) {
        return false;
    }
    reset();
    runtime.clearInactiveFlags(actor());
    runtime.initializeActor(actor(), type);
    initializeStrategy(strategies.select(actor().type(), source));
    initializeLife();
    runtime.writeColor(actor(), runtime.color(type));
    owned.attach(acquired);
    return true;
}

void characterowner::Owner::deactivate() {
    owned.deactivate(runtime.links(actor()));
}

void characterowner::Owner::beginStrategy() {
    if (!owned.strategy) {
        throw std::runtime_error("Original character strategy is null at begin.");
    }
    owned.strategy->begin();
}

void characterowner::Owner::updateStrategy() {
    if (values.flags904[0] || values.field91c) return;
    if (values.field8ea) {
        values.embeddedStrategy.update();
        if (values.embeddedStrategy.state == 0x100) {
            values.field8ea = false;
            nextStrategyState(0);
        }
    } else if (ownerKind == Kind::Ordinary) {
        if (!owned.strategy) throw std::runtime_error("Original character strategy is null at update.");
        values.field958 = values.field954;
        owned.strategy->update();
        runtime.updateEffects(*this);
    }
}

bool characterowner::Owner::beginReplacement() {
    if (values.field8ea) return false;
    if (!owned.strategy) throw std::runtime_error("Original replacement has no primary strategy.");
    values.field8ea = true;
    values.embeddedStrategy.setNextState(0);
    values.embeddedStrategy.owner = owned.strategy->owner;
    values.embeddedStrategy.context = owned.strategy->context;
    return true;
}

void characterowner::Owner::replaceStrategy(unsigned type) {
    initializeStrategy(type);
}

void characterowner::Owner::nextStrategyState(std::uint16_t state) {
    if (!owned.strategy) {
        throw std::runtime_error("Original character strategy is null at state change.");
    }
    owned.strategy->setNextState(state);
}

void characterowner::Owner::update() {
    runtime.update(*this);
}

void characterowner::Owner::setLife(std::uint32_t value) {
    dynamic_cast<actors::MoveActor&>(actor()).life = static_cast<std::uint16_t>(value);
    values.life = value;
}

void characterowner::Owner::initializeLife() { runtime.creationEvent(*this); }

std::uint32_t characterowner::Owner::event(characterevent::Event& event) {
    return runtime.event(*this, event);
}

characterowner::Kind characterowner::Owner::kind() const {
    return ownerKind;
}

enemies::Manager& characterowner::Owner::manager() {
    if (!parent) {
        throw std::runtime_error("Original character owner has no attached manager.");
    }
    return *parent;
}

characterowner::State& characterowner::Owner::state() {
    return values;
}

characterownership::Ownership& characterowner::Owner::ownership() {
    return owned;
}

jmp::ToolData& characterowner::Owner::path() {
    if (!pathData) {
        throw std::runtime_error("Original null character owner has no path table.");
    }
    return *pathData;
}

bool characterowner::Owner::loadPath(const mission::Resources& resources) {
    std::string_view name;
    if (!source.table || source.row < 0 || !source.table->getValue(source.row, "path_name", name)) return false;
    if (const auto* resource = resources.getPathResource(name)) path() = *resource;
    else path().reset();
    return true;
}

std::unique_ptr<enemies::Owner> characterowner::construct(unsigned constructor, const enemystrategy::Catalog& strategies, Runtime& runtime) {
    if (constructor == glme01::nullCharacterConstructor) {
        return std::make_unique<Owner>(Kind::Null, strategies, runtime);
    }
    if (constructor == glme01::ordinaryCharacterConstructor) {
        return std::make_unique<Owner>(Kind::Ordinary, strategies, runtime);
    }
    throw std::runtime_error("Unsupported original character owner constructor.");
}
