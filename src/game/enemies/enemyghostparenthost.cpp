#include "game/enemies/enemyghostparenthost.h"
#include "game/enemies/enemyappearancestrategy.h"
#include "game/enemies/enemysearchstrategy.h"
#include "game/world/mapcollisionstate.h"
#include "game/events/jmptooldata.h"
#include "game/enemies/enemyghostchakasu.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    constexpr unsigned chakasuAttackStrategy = 0x0c;

    class Hosted final : public ghostparent::Parent {
    public:
        explicit Hosted(const ghostparent::Environment& value) : environment(value) {
            // 800C9BD0: the training ghost overrides movement 1, search 5 and condition 2 (setters 800C9080/9088/9090).
            fields.overrides.movement = 1;
            fields.overrides.search = 5;
            fields.overrides.condition = 2;
        }

        void begin() override {
            if (!owner) throw std::runtime_error("Original ghost parent has no character owner.");
            view = std::make_unique<ghostparent::OwnerView>(environment.view ? environment.view(*owner) :
                ghostparent::viewOf(dynamic_cast<characterowner::Owner&>(*owner), environment.actors));
            auto& actor = view->actor;
            auto& state = view->state;
            const auto projectFloor = [this](std::array<float, 3>& position) { environment.collision.projectFloor(position); };
            disappearanceStrategy = std::make_unique<enemystrategy::GhostDisappearance>(enemystrategy::DisappearanceServices{
                actor, state, environment.catalog, environment.models, projectFloor, environment.particle,
                view->releaseShared, view->releaseReference, [this] { completeDisappearance(); }});
            ghostparent::ParentServices services{actor, state, view->manager, environment.types.parameters(actor.type()),
                owner->source, environment.pose, *disappearanceStrategy,
                [this] { return environment.runtime.random(); },
                [this, &actor, &state](const ghostparent::Selection& selection) { return assemble(actor, state, selection); },
                [this](unsigned group) { return environment.runtime.roomCleared(group); },
                [](std::uint32_t) {
                    throw std::runtime_error("Original ghost element effect (800BEA6C) is not ported.");
                },
                [this, &actor, &state]() { return makeCapture(actor, state); },
                [this](characterevent::Event& event) {
                    if (auto* observer = dynamic_cast<characterevent::Observer*>(children.attack)) observer->fishingBegin(event);
                },
                [this] {
                    if (secondDelegate) throw std::runtime_error("Original ghost secondary delegate (800DC5E8) is not ported.");
                },
                [] {},
                [this, &actor, &state] {
                    view->event(8u);
                    if (actor.selectAnimation("NONE")) {
                        state.field8cc = 0;
                        state.field8d0 = 0;
                    }
                },
                [] { throw std::runtime_error("Original ghost secondary result (800F4374) is not ported."); },
                [this, &actor](unsigned player) { damage(actor, player); },
                [this, &actor, &state]() -> enemystrategy::Strategy* {
                    if (!lightStrategy) lightStrategy = makeLight(actor, state);
                    return lightStrategy.get();
                }};
            bind(std::move(services));
            Parent::begin();
        }
    private:
        const ghostparent::Environment& environment;
        std::unique_ptr<ghostparent::OwnerView> view;
        std::unique_ptr<enemystrategy::GhostDisappearance> disappearanceStrategy;
        std::unique_ptr<enemystrategy::Strategy> lightStrategy;
        std::unique_ptr<ghostcapture::Collision> tracker;
        std::unique_ptr<ghostcapture::Bookkeeping> bookkeeping;
        std::array<float, 3> captureSourcePosition{};
        std::array<std::unique_ptr<enemystrategy::Strategy>, 5> storage;

        static float number(std::uint32_t bits) {
            float value;
            std::memcpy(&value, &bits, sizeof value);
            return value;
        }

        const gameparameters::Parameter& lightParameter(unsigned type, unsigned index, const char* name) const {
            const auto& entry = environment.types.parameters(type).entries().at(index);
            if (entry.name != name) throw std::runtime_error("Original EnLight parameter does not match the native parameter list.");
            return entry;
        }

        std::unique_ptr<enemystrategy::Strategy> makeLight(actors::MoveActor& actor, characterowner::State& state) {
            if (!view->ownership || !view->dispatch || !view->beginLightEffect)
                throw std::runtime_error("Original EnLight owner services are not bound.");
            auto light = std::make_unique<enemystrategy::Light>(enemystrategy::LightServices{
                actor, state, *view->ownership, environment.trigonometry,
                [this](unsigned type) { return lightParameter(type, 6, "mLightBindFrame").bits; },
                [this](unsigned type) { return number(lightParameter(type, 7, "mMinLightBindRange").bits); },
                [this](unsigned type) { return number(lightParameter(type, 8, "mMaxLightBindRange").bits); },
                [this] { return view->manager.playerPosition(0); },
                [this, &actor] { return environment.creation.group(actor.type()) == 3; },
                environment.reducedScale,
                view->dispatch,
                view->beginLightEffect,
                [this](unsigned player, unsigned kind, std::uint32_t target) {
                    playerevents::Payload request;
                    request.word(0x38, target);
                    environment.events.send(static_cast<std::int32_t>(player), kind, &request);
                },
                view->releaseReference,
                [this](std::array<float, 3>& position) { environment.collision.projectFloor(position); },
                environment.particle, environment.sound});
            return light;
        }

        std::unique_ptr<enemystrategy::Strategy> makeCapture(actors::MoveActor& actor, characterowner::State& state) {
            if (!tracker) tracker = std::make_unique<ghostcapture::Collision>(environment.shapes, environment.pose, environment.rooms,
                environment.collision);
            if (!bookkeeping) bookkeeping = std::make_unique<ghostcapture::Bookkeeping>(environment.executable, environment.creation,
                environment.drops);
            return std::make_unique<enemystrategy::Capture>(enemystrategy::CaptureServices{
                actor, state, environment.collision, environment.pose, environment.types, *tracker, *bookkeeping,
                [this] { return environment.runtime.random(); }, environment.frame,
                [this, &actor]() -> ghostcapture::Vector {
                    ghostcapture::Vector delta{};
                    view->manager.nearestPlayer(actor.position, &delta);
                    return delta;
                },                [this, &state]() -> const ghostcapture::Vector* {
                    if (!state.captureRecord) return nullptr;
                    captureSourcePosition = environment.player(state.field950).position;
                    return &captureSourcePosition;
                },
                view->releaseReference}, environment.common);
        }
        void damage(actors::MoveActor& actor, unsigned player) {
            // 800E730C: damage message 3 carrying the ghost actor and its parameter record word at +0x30.
            const auto& entry = environment.types.parameters(actor.type()).entries().at(1);
            if (entry.name != "mHitDamage") throw std::runtime_error("Original ghost damage parameter does not match the native parameter list.");
            playerevents::Payload payload;
            payload.word(0x14, actor.id());
            payload.word(0x18, static_cast<std::uint32_t>(entry.bits));
            payload.word(0x20, 3);
            environment.events.send(static_cast<std::int32_t>(player), 3, &payload);
        }

        template <class Child>
        enemystrategy::Strategy* adopt(unsigned slot, std::unique_ptr<Child> child) {
            if (!child) return nullptr;
            child->owner = owner;
            child->context = context;
            storage[slot] = std::move(child);
            return storage[slot].get();
        }

        std::unique_ptr<enemystrategy::Strategy> movementObject(actors::MoveActor& actor, characterowner::State& state,
            ghostparent::MovementChild kind) {
            const enemystrategy::RandomMovementServices movement{actor, state, owner->source, environment.collision,
                environment.pose, [this] { return environment.runtime.random(); }, environment.frame};
            switch (kind) {
            case ghostparent::MovementChild::Random: return std::make_unique<enemystrategy::RandomMovement>(movement);
            case ghostparent::MovementChild::SearchFar:
                return std::make_unique<enemystrategy::NearSearch>(enemystrategy::SearchServices{movement, environment.player}, 500.0f);
            case ghostparent::MovementChild::SearchNear:
                return std::make_unique<enemystrategy::NearSearch>(enemystrategy::SearchServices{movement, environment.player}, 200.0f);
            case ghostparent::MovementChild::Default: return std::make_unique<enemystrategy::Strategy>();
            }
            throw std::invalid_argument("Unknown ghost movement child.");
        }

        ghostparent::Children assemble(actors::MoveActor& actor, characterowner::State& state, const ghostparent::Selection& selection) {
            ghostparent::Children result;
            result.movement = adopt(0, movementObject(actor, state, selection.movement));
            result.search = adopt(1, movementObject(actor, state, selection.search));
            switch (selection.appearance) {
            case ghostparent::AppearanceChild::None: break;
            case ghostparent::AppearanceChild::Appear2:
                result.appearance = adopt(2, std::make_unique<enemystrategy::Appearance>(enemystrategy::AppearanceServices{
                    actor, state, environment.catalog, environment.models, environment.pose,
                    [this, &actor, &state] { notifyAppearance(actor, state); },
                    [this](std::array<float, 3>& position) { environment.collision.projectFloor(position); },
                    environment.particle, [this] { return environment.player(0).position; }}));
                break;
            case ghostparent::AppearanceChild::Frighten:
                throw std::runtime_error("Original ghost Fright appearance services are not bound.");
            case ghostparent::AppearanceChild::Appear6:
                throw std::runtime_error("Original ghost appearance type 6 is not ported.");
            }
            result.condition = adopt(3, environment.factory.construct(environment.strategies.constructor(selection.condition)));
            if (selection.attack != chakasuAttackStrategy)
                throw std::runtime_error("Original ghost attack strategy " + std::to_string(selection.attack) + " is not ported.");
            result.attack = adopt(4, std::make_unique<ghostparent::ChakasuAttack>(ghostparent::ChakasuServices{
                actor, state, environment.collision, environment.pose, environment.player, environment.frame}));
            return result;
        }

        void notifyAppearance(actors::MoveActor& actor, characterowner::State& state) {
            // 800E7378: scare message 2 to the owner's player, only while the ghost's room is visible.
            if (!view->manager.roomVisible(actor.resolvePositionRoom())) return;
            playerevents::Payload payload;
            payload.word(0x14, actor.id());
            payload.word(0x18, 0);
            environment.events.send(static_cast<std::int32_t>(state.field950), 2, &payload);
        }
    };
}

ghostparent::OwnerView ghostparent::viewOf(characterowner::Owner& owner, const actors::Registry& registry) {
    return OwnerView{dynamic_cast<actors::MoveActor&>(owner.actor()), owner.state(), owner.manager(),
        [&owner] {
            auto& owned = owner.ownership();
            if (owned.sharedUsers[0]) owned.releaseShared(owned.sharedUsers[0]);
        },
        [&owner](void*& user) { owner.releaseActorReference(user); },
        [&owner](unsigned message) { owner.event(message); },
        &owner.ownership(),
        [&owner](characterevent::Event& event) { return owner.event(event); },
        [&owner, &registry] { enemystrategy::beginLightEffect(owner, registry); }};
}
std::unique_ptr<enemystrategy::Strategy> ghostparent::construct(const Environment& environment) {
    return std::make_unique<Hosted>(environment);
}