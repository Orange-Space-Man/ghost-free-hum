#include "game/enemies/enemyghostparenthost.h"
#include "game/enemies/enemyappearancestrategy.h"
#include "game/enemies/enemysearchstrategy.h"
#include "game/world/mapcollisionstate.h"
#include "game/events/jmptooldata.h"
#include "game/enemies/enemyghostchakasu.h"
#include "game/enemies/enemyghostpunch.h"
#include "game/enemies/enemyghostbanaoba.h"
#include "game/enemies/enemyghostupper.h"
#include "game/actors/actortimingsampler.h"
#include "game/player/playeractor.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    constexpr unsigned chakasuAttackStrategy = 0x0c;

    class FindPlayer final : public enemystrategy::Strategy {
    public:
        FindPlayer(const ghostparent::Environment& value, float maximum) : environment(value), maximum(maximum) {}
        void behavior() override {
            if (!owner) throw std::runtime_error("EnFindPlayerInf requires its ghost owner.");
            auto& character = dynamic_cast<characterowner::Owner&>(*owner);
            auto& actor = dynamic_cast<actors::MoveActor&>(character.actor());
            const auto& parameters = environment.types.parameters(actor.type());
            std::array<float, 3> displacement{};
            const int index = ghostparent::sightQuery(character.manager(), actor,
                ghostparent::parameterWord(parameters, ghostparent::Parameter::Eyesight), &displacement);
            if (index < 0) return;
            const float range = std::sqrt(displacement[0] * displacement[0] + displacement[2] * displacement[2]);
            if (!(range >= 100.0f && range <= maximum)) return;
            auto& player = dynamic_cast<playeractor::Player&>(environment.player(static_cast<unsigned>(index)));
            const auto& direction = player.state.direction;
            const float facing = (actor.position[0] - player.position[0]) * direction[0] +
                (actor.position[1] - player.position[1]) * direction[1] +
                (actor.position[2] - player.position[2]) * direction[2];
            if (facing < 0.0f) setNextState(0x100);
        }
    private:
        const ghostparent::Environment& environment;
        float maximum;
    };

    class Hosted final : public ghostparent::Parent {
    public:
        Hosted(const ghostparent::Environment& value, bool trainingOverrides) : environment(value) {
            if (trainingOverrides) {
                // 800C9BD0 installs these three overrides after the base ghost constructor.
                fields.overrides.movement = 1;
                fields.overrides.search = 5;
                fields.overrides.condition = 2;
            }
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
                [this, &actor](int kind, std::int32_t& previous) { place(actor, kind, previous); },
                [this, &actor, &state](const ghostparent::Selection& selection) { return assemble(actor, state, selection); },
                [this](unsigned group) { return environment.runtime.roomCleared(group); },
                [](std::uint32_t element) {
                    // 800BEADC takes no effect path when mElement is zero.
                    if (element != 0)
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

        void place(actors::MoveActor& actor, int kind, std::int32_t& previous) {
            // 800C9354 / 800C93D0 load path_name and sample its pnt0 coordinate columns.
            auto& character = dynamic_cast<characterowner::Owner&>(*owner);
            if (!character.loadPath(environment.mission) || !character.path().attached())
                throw std::runtime_error("Original ghost placement path is absent.");
            const auto& path = character.path();
            const unsigned count = path.count();
            if ((kind == 1 && count == 0) || (kind == 2 && count < 2))
                throw std::runtime_error("Original ghost placement path has too few points.");
            const auto point = [&path](unsigned index) {
                std::array<float, 3> value{};
                const char* columns[]{"pnt0_x", "pnt0_y", "pnt0_z"};
                for (unsigned axis = 0; axis < 3; ++axis)
                    if (!path.getValue(index, columns[axis], value[axis]))
                        throw std::runtime_error("Original ghost placement path coordinate is absent.");
                return value;
            };
            std::array<float, 3> position{};
            if (kind == 1) {
                unsigned index = environment.runtime.random() % count;
                if (previous >= 0 && index == static_cast<unsigned>(previous)) index = (index + 1) % count;
                previous = static_cast<std::int32_t>(index);
                position = point(index);
            } else {
                const unsigned index = environment.runtime.random() % (count - 1);
                const float fraction = static_cast<float>(environment.runtime.random() % 10000) / 9999.0f;
                const auto first = point(index), second = point(index + 1);
                for (unsigned axis = 0; axis < 3; ++axis)
                    position[axis] = first[axis] + fraction * (second[axis] - first[axis]);
            }
            actor.setPosition(position);
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
            if (selection.attack == chakasuAttackStrategy) {
                result.attack = adopt(4, std::make_unique<ghostparent::ChakasuAttack>(ghostparent::ChakasuServices{
                    actor, state, environment.collision, environment.pose, environment.player, environment.frame}));
            } else if (selection.attack == 0x26) {
                gameparameters::Parameters upper;
                upper.add("mDamage", 4, 10);
                upper.add("mAttackType", 2, 2);
                const int entry = environment.common.findPathResource("/param/th/AEnAtUpper.prm");
                if (entry >= 0) {
                    std::string error;
                    if (!upper.load(environment.common.resource(entry), error)) throw std::runtime_error(error);
                }
                const std::uint32_t damageAmount = upper.entries()[0].bits;
                const std::uint16_t attackType = static_cast<std::uint16_t>(upper.entries()[1].bits);
                const auto& disappear = environment.types.parameters(actor.type()).entries().at(14);
                if (disappear.name != "mDisappearFrame") throw std::runtime_error("Original ghost disappearance parameter is out of place.");
                result.attack = adopt(4, std::make_unique<ghostparent::UpperAttack>(ghostparent::UpperServices{
                    actor, state, environment.collision, environment.pose, environment.shapes, environment.trails,
                    environment.player, environment.frame,
                    [this] { return environment.runtime.random(); }, disappear.bits,
                    [this, &actor](unsigned player) { damage(actor, player); },
                    [this, &actor, damageAmount, attackType](unsigned player) {
                        constexpr std::uint32_t attackKinds[]{6, 7, 5};
                        if (attackType >= 3) throw std::runtime_error("Original AEnAtUpper attack type is out of range.");
                        playerevents::Payload payload;
                        payload.word(0x14, actor.id());
                        payload.word(0x18, damageAmount);
                        payload.bytes[0x1c] = 1;
                        payload.word(0x20, attackKinds[attackType]);
                        return environment.events.send(static_cast<std::int32_t>(player), 3, &payload);
                    }}));
            } else if (selection.attack == 6) {
                // 800DDF9C constructs EnAtkPunch with the original attack parameter record.
                gameparameters::Parameters punch;
                punch.add("mDamage", 4, 10);
                punch.add("mAttackType", 2, 2);
                punch.add("mFlag", 2, 0);
                const int entry = environment.common.findPathResource("/param/th/EnAtkPunch.prm");
                if (entry >= 0) {
                    std::string error;
                    if (!punch.load(environment.common.resource(entry), error)) throw std::runtime_error(error);
                }
                const std::uint32_t damageAmount = punch.entries()[0].bits;
                const std::uint16_t attackType = static_cast<std::uint16_t>(punch.entries()[1].bits);
                result.attack = adopt(4, std::make_unique<ghostparent::PunchAttack>(ghostparent::PunchServices{
                    actor, state, environment.collision, environment.pose, environment.player, environment.frame,
                    [this, &actor](unsigned player) { damage(actor, player); },
                    [this, &actor, damageAmount, attackType](unsigned player) {
                        // 800DDE3C sends event 3 with the punch damage and mapped attack kind.
                        constexpr std::uint32_t attackKinds[]{6, 7, 5};
                        if (attackType >= 3) throw std::runtime_error("Original EnAtkPunch attack type is out of range.");
                        playerevents::Payload payload;
                        payload.word(0x14, actor.id());
                        payload.word(0x18, damageAmount);
                        payload.bytes[0x1c] = 1;
                        payload.word(0x20, attackKinds[attackType]);
                        environment.events.send(static_cast<std::int32_t>(player), 3, &payload);
                    },
                    [this, &actor] {
                        // 800DE84C samples the animation's "attack" track above 0.5.
                        const auto model = environment.catalog.type(actor.type()).model;
                        const auto timing = environment.models.timing(model, actor.keyIndex & 0xffff);
                        return actoranimation::sampleTiming(timing.get(), "attack", actor.animationFrame) > 0.5f;
                    }}));
            } else if (selection.attack == 0x65) {
                // 800DF040 / 800DF404: EnAtkBanaoba uses the EAT birth track and
                // creates EnBanana (type 0xA8) through the original enemy manager.
                gameparameters::Parameters banana;
                banana.add("mDamage", 4, 0);
                banana.add("mAttackType", 2, 0);
                banana.add("mDirRange", 4, 0x3f400000);
                banana.add("mPower", 4, 0x41a00000);
                const int entry = environment.common.findPathResource("/param/th/EnAtkBanaoba.prm");
                if (entry >= 0) {
                    std::string error;
                    if (!banana.load(environment.common.resource(entry), error)) throw std::runtime_error(error);
                }
                const float range = number(banana.entries()[2].bits);
                const float power = number(banana.entries()[3].bits);
                result.attack = adopt(4, std::make_unique<ghostparent::BanaobaAttack>(ghostparent::BanaobaServices{
                    actor, state,
                    [this, &actor] {
                        const auto model = environment.catalog.type(actor.type()).model;
                        const auto timing = environment.models.timing(model, actor.keyIndex & 0xffff);
                        return actoranimation::sampleTiming(timing.get(), "birth", actor.animationFrame);
                    },
                    [this, &actor, range, power] {
                        const auto id = view->manager.createActor(0xa8, -1);
                        if (static_cast<std::int32_t>(id) < 0) return;
                        auto* bananaActor = environment.actors.actor(id);
                        if (!bananaActor || !actor.model) throw std::runtime_error("EnAtkBanaoba created an absent banana actor.");
                        const auto joint = dooranimation::concatenate(actor.matrix, actor.model->matrix(0, 0x19));
                        bananaActor->setPosition({joint[3], joint[7], joint[11]});
                        auto direction = environment.pose.horizontalDirection(static_cast<std::uint16_t>(actor.rotation[1]));
                        direction[0] = (direction[0] * (1.0f - range)) * power;
                        direction[1] = range * power;
                        direction[2] = (direction[2] * (1.0f - range)) * power;
                        bananaActor->setVelocity(direction);
                    }}));
            } else {
                throw std::runtime_error("Original ghost attack strategy " + std::to_string(selection.attack) + " is not ported.");
            }
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
std::unique_ptr<enemystrategy::Strategy> ghostparent::construct(const Environment& environment, bool trainingOverrides) {
    return std::make_unique<Hosted>(environment, trainingOverrides);
}
std::unique_ptr<enemystrategy::Strategy> ghostparent::constructFindPlayer(const Environment& environment) {
    return std::make_unique<FindPlayer>(environment, 3.4028234663852886e+38f);
}
std::unique_ptr<enemystrategy::Strategy> ghostparent::constructFindPlayerNear(const Environment& environment) {
    return std::make_unique<FindPlayer>(environment, 250.0f);
}
