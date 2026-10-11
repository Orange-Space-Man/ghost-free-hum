#include "game/actors/characterownership.h"
#include "game/actors/characterevent.h"
#include "game/enemies/enemystrategydecorator.h"
#include "game/world/roomlightpool.h"
#include "game/audio/jaudiopositionowners.h"
#include "game/effects/particleparameters.h"
#include "game/effects/particlepoolstate.h"
#include "game/effects/particleemitterstate.h"
#include "game/effects/modeleffectpool.h"
#include "game/effects/sharedmodeleffect.h"
#include "game/effects/bridgeeffectpool.h"
#include "game/effects/icejaileffectpool.h"
#include "game/effects/icicleeffectpool.h"
#include "game/effects/waterfalleffectpool.h"
#include "game/player/playericeeffectpool.h"
#include "game/effects/barriereffectpool.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct PositionalSound : jaudio::PositionalSound {
        std::uint32_t properties = 0;
        unsigned stops = 0;
        void stop(unsigned frames) override {
            require(frames == 0, "Position-owner release stops immediately.");
            ++stops;
            if (binding) *binding = nullptr;
            next = nullptr;
        }
        std::uint32_t flags() const override { return properties; }
    };

    struct Resource : characterownership::Resource {
        std::vector<unsigned>& calls;
        unsigned id, queries = 0;
        int state = 0;
        Resource(std::vector<unsigned>& calls, unsigned id) : calls(calls), id(id) {}
        void release() override { calls.push_back(id); }
        int attachmentState() override { ++queries; return state; }
        void setAttachmentPosition(const std::array<float, 3>&, const std::array<float, 3>&) override {}
        void setMatrix(const std::array<float, 12>&) override {}
        void setVisible(bool) override {}
        void setRunning(bool) override {}
    };

    struct Resources : characterownership::Resources {
        std::vector<unsigned> calls;
        Resource first{calls, 1}, second{calls, 2};
        bool firstMissing = false, secondMissing = false;
        std::int32_t value = 0;
        unsigned variant = 0;
        characterownership::Resource* createPrimary(unsigned type, const std::array<float, 3>& position) override {
            require(position == std::array<float, 3>{}, "Primary resource uses the original zero position.");
            calls.push_back(0x10000 | type);
            return firstMissing ? nullptr : &first;
        }
        characterownership::Resource* createSecondary(unsigned type, const std::array<float, 3>& position,
            const std::array<float, 3>& vector) override {
            require(position == std::array<float, 3>{} && vector == std::array<float, 3>{}, "Secondary resource uses two zero vectors.");
            calls.push_back(0x20000 | type);
            return secondMissing ? nullptr : &second;
        }
        std::int32_t playerValue() override { calls.push_back(0x30000); return value; }
        unsigned gameVariant() override { calls.push_back(0x40000); return variant; }
    };

    struct Strategy : characterownership::Strategy {
        std::vector<unsigned>& calls;
        explicit Strategy(std::vector<unsigned>& calls) : calls(calls) {}
        ~Strategy() override { calls.push_back(10); }
    };

    struct Actor : characterownership::ActorLinks {
        std::vector<unsigned>& calls;
        std::array<void*, 7> children{};
        bool retainChild = false;
        explicit Actor(std::vector<unsigned>& calls) : calls(calls) {}
        void setInactiveFlags() override { calls.push_back(11); }
        void* child(unsigned index) override { return children.at(index); }
        void detach(void* value) override {
            calls.push_back(12);
            if (!retainChild) for (auto& child : children) if (child == value) { child = nullptr; break; }
        }
        void deactivate(void*) override { calls.push_back(13); }
        void releaseAudio() override { calls.push_back(14); }
    };

    struct StateStrategy : enemystrategy::Strategy {
        std::vector<unsigned> calls;
        bool queueDuringInit = false;
        void initializeBehavior() override {
            require(nextState == 0xffff && frames == 0, "State transition resets pending state and timer before initialization.");
            calls.push_back(state);
            if (queueDuringInit) {
                setNextState(7);
            }
        }
        void behavior() override {
            calls.push_back(0x10000 | state);
        }
    };
}

int main() {
    try {
        StateStrategy strategy;
        require(!strategy.owner && !strategy.context && strategy.state == 0 && strategy.nextState == 0 && strategy.frames == 0,
            "Original base strategy constructor state.");
        strategy.update();
        require(strategy.calls == std::vector<unsigned>{0, 0x10000} && strategy.frames == 1 && strategy.nextState == 0xffff,
            "First update initializes state zero before behavior and increments frames afterward.");
        strategy.calls.clear();
        strategy.update();
        require(strategy.calls == std::vector<unsigned>{0x10000} && strategy.frames == 2, "Sentinel suppresses repeated initialization.");
        strategy.setNextState(3);
        strategy.queueDuringInit = true;
        strategy.update();
        require(strategy.state == 3 && strategy.nextState == 7 && strategy.frames == 1, "A transition queued during initialization waits for the next update.");
        strategy.queueDuringInit = false;
        strategy.update();
        require(strategy.state == 7 && strategy.nextState == 0xffff && strategy.frames == 1, "Deferred transition executes on the following update.");
        strategy.frames = 0xffffffff;
        strategy.update();
        require(strategy.frames == 0, "Original strategy frame counter wraps at 32 bits.");
        enemystrategy::Decorator decorator;
        decorator.update();
        require(decorator.frames == 1 && decorator.nextState == 0xffff && decorator.result == 0x100,
            "An inactive decorator executes its own original base state machine.");
        for (const auto terminal : {0xff, 0x100, 0x101, 0x102, 0x103}) {
            StateStrategy child;
            child.setNextState(static_cast<std::uint16_t>(terminal));
            decorator.active = &child;
            decorator.result = 0x4321;
            decorator.update();
            const bool ended = terminal >= 0x100 && terminal <= 0x102;
            require(decorator.frames == 1 && child.frames == 1 &&
                decorator.active == (ended ? nullptr : &child) && decorator.result == (ended ? terminal : 0x4321),
                "Decorator stops its own clock while delegated and releases only states 100 through 102.");
            decorator.active = nullptr;
        }
        const unsigned cases[][3] = {
            {0x75, 0x1d, 0x56}, {0x76, 0x1e, 0x57}, {0x77, 0x1f, 0x58},
            {0xc2, 0x16, 0x4e}, {0xc3, 0x17, 0x4f}, {0xc4, 0x18, 0x50},
            {0xc5, 0x15, 0x4d}, {0xc6, 0x13, 0x4b}, {0xc7, 0x14, 0x4c},
            {0xc8, 0x0f, 0x47}, {0xc9, 0x10, 0x48}, {0xca, 0x11, 0x49},
            {0xcb, 0x12, 0x4a}, {0xcc, 0x1c, 0x4c}
        };
        for (const auto& test : cases) for (unsigned missing = 0; missing < 4; ++missing) {
            Resources resources;
            resources.firstMissing = (missing & 1) != 0;
            resources.secondMissing = (missing & 2) != 0;
            characterownership::Ownership owner;
            characterownership::Acquired acquired;
            require(owner.acquire(test[0], resources, acquired) == (missing == 0), "Both required resources determine activation success.");
            std::vector<unsigned> expected{0x10000 | test[1], 0x20000 | test[2]};
            if (missing) {
                if (!resources.firstMissing) expected.push_back(1);
                if (!resources.secondMissing) expected.push_back(2);
            }
            require(resources.calls == expected, "Factories run in original order even if first fails; partial resources are released.");
        }
        for (unsigned type = 0; type < 381; ++type) {
            bool special = type == 0xbd || type == 0xcd || type == 0xce || (type >= 0x56 && type <= 0x5a);
            for (const auto& test : cases) special |= type == test[0];
            if (special) continue;
            Resources resources;
            const auto result = characterownership::acquire(type, resources);
            require(result.success && !result.first && !result.second && resources.calls.empty(), "Ordinary types need no special resource.");
        }
        for (unsigned type = 0x56; type <= 0x5a; ++type) {
            Resources resources;
            resources.secondMissing = true;
            const auto result = characterownership::acquire(type, resources);
            require(result.success && !result.second && resources.calls == std::vector<unsigned>{0x20000U | (type == 0x56 ? 0x3cU : 0x3dU)},
                "Original optional secondary resource failure does not reject activation.");
        }
        for (unsigned variant = 0; variant < 2; ++variant) for (int delta = -1; delta < 2; ++delta) {
            Resources resources;
            resources.variant = variant;
            resources.value = (variant ? 400 : 200) + delta;
            require(characterownership::acquire(0xcd, resources).success && resources.calls ==
                std::vector<unsigned>{0x40000, 0x30000, 0x10000U | (delta < 0 ? 0x19U : 0x1aU)}, "Player threshold preserves equality branch and variant.");
        }
        Resources resources;
        resources.value = -1;
        characterownership::acquire(0xcd, resources);
        require(resources.calls.back() == 0x10019, "Player threshold uses signed comparison.");
        resources.calls.clear();
        require(characterownership::acquire(0xbd, resources).first == &resources.second &&
            resources.calls == std::vector<unsigned>{0x2003c}, "Type BD places a secondary-factory resource in the first output.");
        resources.calls.clear();
        require(characterownership::acquire(0xce, resources).first == &resources.first &&
            resources.calls == std::vector<unsigned>{0x1001b}, "Type CE uses only the primary factory.");
        resources.calls.clear();
        {
            characterownership::Ownership owner;
            owner.attach({&resources.first, &resources.second, true});
            require(owner.attachmentCount == 2 && owner.attachments[0].resource == &resources.first &&
                owner.attachments[1].resource == &resources.second && owner.attachments[0].position == std::array<float, 3>{},
                "Resources attach in output order with zero flags and offsets.");
            resources.first.state = 1;
            owner.attach({&resources.first, nullptr, true});
            require(owner.attachmentCount == 2, "Nonzero attachment state skips insertion.");
            resources.first.state = 0;
            owner.attach({&resources.first, &resources.first, true});
            owner.attach({&resources.first, &resources.first, true});
            require(owner.attachmentCount == 5 && resources.first.queries == 6, "Capacity is five; state query still occurs for the sixth insertion.");
            owner.reset();
            require(owner.attachmentCount == 0 && owner.attachments[0].resource == &resources.first && resources.calls.empty(),
                "Reset clears the attachment count without releasing or overwriting retained records.");
            owner.attach({&resources.first, &resources.second, true});
            owner.strategy = std::make_unique<Strategy>(resources.calls);
            Resource shared(resources.calls, 3);
            int firstChild = 0, secondChild = 0;
            owner.sharedUsers = {&firstChild, nullptr, &secondChild};
            owner.sharedCount = 2; owner.sharedResource = &shared;
            owner.dependentActor = &firstChild;
            Actor actor(resources.calls);
            actor.children[0] = &firstChild; actor.children[6] = &secondChild;
            owner.deactivate(actor);
            require(resources.calls == std::vector<unsigned>{10, 11, 3, 1, 2, 12, 13, 12, 13, 14},
                "Teardown destroys strategy, sets flags, releases shared/attached resources, detaches children, then releases audio.");
            require(!owner.strategy && !owner.sharedResource && owner.sharedCount == 0 && !owner.dependentActor && owner.attachmentCount == 2,
                "Teardown retains the original attachment count while clearing shared ownership and dependent pointer.");
            resources.calls.clear();
            owner.reset();
            owner.dependentActor = &firstChild;
            actor.children[0] = &firstChild;
            actor.retainChild = true;
            owner.deactivate(actor);
            require(resources.calls == std::vector<unsigned>{11, 12, 13, 13, 14},
                "Failed child detach leaves the dependent child eligible for the original second deactivation.");
            resources.calls.clear();
            owner.strategy = std::make_unique<Strategy>(resources.calls);
            owner.attach({&resources.first, nullptr, true});
        }
        require(resources.calls == std::vector<unsigned>{10}, "Owner destruction deletes strategy but is not actor deactivation or resource release.");
        roomlighting::Pool lights;
        {
            struct Archive : particles::ArchiveResources {
                std::vector<std::string> names;
                particles::Resource get(std::uint32_t type, std::string_view name) override {
                    require(type == 0x4b485f45, "Waterfall particles use their original archive type.");
                    names.emplace_back(name);
                    return std::make_shared<std::vector<std::uint8_t>>(0xc0);
                }
            } archive;
            gameflags::State flags;
            std::vector<unsigned> sounds;
            const std::array<float, 3> position{12, 34, 56};
            auto sound = [&](unsigned id, const std::array<float, 3>& value, unsigned extra) {
                require(value == position && extra == 0, "Waterfall sound retains the allocation position and zero argument.");
                sounds.push_back(id);
            };
            for (unsigned capacity = 0; capacity < 3; ++capacity) {
                particles::EmitterPool particles(capacity, 0, 0);
                modeleffects::WaterfallPool waterfall(archive, particles, flags, sound);
                auto& owner = static_cast<modeleffects::WaterfallOwner&>(*waterfall.owners[0]);
                auto model = std::make_shared<j3dmodel::ModelData>();
                owner.model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
                modeleffects::Factory factory;
                factory.add(waterfall);
                archive.names.clear();
                auto* result = factory.create(0x27, position);
                require(archive.names == std::vector<std::string>{"taki_smoke.pcr", "ice_smok.bti", "taki_shibuki.pcr", "watball1.bti"} &&
                    particles.attempts == 2 && bool(result) == (capacity == 2),
                    "Both waterfall particle acquisitions run in order, including first-allocation failure.");
                if (capacity < 2) {
                    require(!owner.emitters[0] && !owner.emitters[1] && !(owner.flags & 1),
                        "Partial waterfall allocation releases emitters without activating the model.");
                    if (capacity) require(particles.emitters.at(0).flags == 0x1c, "Partial waterfall emitter uses deferred release.");
                    continue;
                }
                require(owner.position == position && owner.model->matrix[3] == 12 &&
                    owner.emitters[0]->position == position && owner.emitters[1]->position == position,
                    "Placement reaches particle creation before the factory activates the model.");
                waterfall.textureAnimation = std::make_shared<j3dmodel::Animation>();
                waterfall.textureAnimation->frameCount = 4;
                waterfall.advance(owner, false);
                require(sounds.empty(), "Invisible waterfall does not deliver sound or advance.");
                owner.flags |= 0x10;
                waterfall.advance(owner, true);
                require(sounds == std::vector<unsigned>{0x108c} && owner.frame == 0 && owner.soundFrame == 0,
                    "Waterfall audio continues while animation is paused, without aging the pre-event timer.");
                flags.setOn(0x3f);
                owner.soundFrame = 161;
                waterfall.advance(owner, true);
                waterfall.advance(owner, true);
                require(sounds == std::vector<unsigned>{0x108c, 0x108c, 0x189e} && owner.soundFrame == 163,
                    "Waterfall delivers the transition sound on original tick 162.");
                for (unsigned i = 0; i < 2; ++i) {
                    auto material = std::make_unique<j3dmodel::Material>();
                    material->identityFlags = 0x80000000;
                    material->tev = std::make_unique<j3dmodel::MaterialTevBlock>();
                    material->tev->capacity = 4;
                    material->tev->colors[0] = {10, 20, 30, 1};
                    material->tev->colors[1] = {40, 50, 60, 3};
                    model->materials.entries.push_back(std::move(material));
                }
                waterfall.advance(owner, true);
                require((owner.flags & 1) && model->materials.entries[0]->tev->colors[0][3] == 0,
                    "First waterfall fade stage remains active on the tick alpha reaches zero.");
                waterfall.advance(owner, true);
                std::array<float, 2> rates{};
                for (unsigned i = 0; i < rates.size(); ++i) std::memcpy(&rates[i], &owner.emitters[i]->parameters.fieldF4, sizeof(float));
                require(model->materials.entries[1]->tev->colors[1] == std::array<std::uint16_t, 4>{40, 50, 60, 1} &&
                    rates[0] == 1.75f / 255.0f && rates[1] == 1.5f / 255.0f,
                    "Second waterfall fade stage updates all materials and the two distinct emitter rates.");
                waterfall.advance(owner, true);
                require(owner.flags == 14 && !owner.emitters[0] && !owner.emitters[1] && owner.soundFrame == 163,
                    "Completed fade releases particles and retains the original sound timer across reset.");
                waterfall.advance(owner, false);
                require(owner.flags == 14, "Inactive waterfall retains its release bit.");
                flags.clear();
            }
            particles::EmitterPool particles(2, 0, 0);
            modeleffects::WaterfallPool waterfall(archive, particles, flags, sound);
            const j3dmodel::Matrix matrix{1, 0, 0, 12, 0, 1, 0, 34, 0, 0, 1, 56};
            auto* active = waterfall.available(matrix);
            active->flags |= 1;
            const auto reads = archive.names.size();
            require(!waterfall.available(matrix) && archive.names.size() == reads && (active->flags & 1) &&
                particles.emitters.at(0).flags == 0x1c && particles.emitters.at(1).flags == 0x1c,
                "Requesting an active waterfall releases its emitters without resetting or replacing its model owner.");
            particles::EmitterPool iceEmitters(2, 0, 0);
            modeleffects::PlayerIcePool ice(archive, iceEmitters);
            ice.transform = std::make_shared<j3dmodel::Animation>();
            ice.transform->frameCount = 8;
            ice.textureAnimation = std::make_shared<j3dmodel::Animation>();
            ice.textureAnimation->frameCount = 2;
            auto& frozen = static_cast<modeleffects::PlayerIceOwner&>(*ice.owners[0]);
            frozen.model = std::make_unique<j3dmodel::ModelInstance>(std::make_shared<j3dmodel::ModelData>(), 0x10000);
            modeleffects::Factory iceFactory;
            iceFactory.add(ice);
            archive.names.clear();
            require(iceFactory.create(0xb, position) == &frozen &&
                archive.names == std::vector<std::string>{"lg_eledmg_i_a.pcr", "star6_ex.bti", "lg_eledmg_i_b.pcr", "ice_smok.bti"} &&
                frozen.emitters[0]->position == std::array<float, 3>{},
                "Player ice reads all four resources before creating particles initially at the original zero position.");
            frozen.setRunning(false);
            frozen.setMatrix(matrix);
            ice.advance(frozen, false);
            require(frozen.frame == 0 && frozen.textureFrame == 1 && frozen.emitters[0]->position == position,
                "Player ice tracks position and texture animation outside room visibility while its transform is stopped.");
            frozen.frame = 6;
            ice.advance(frozen, false);
            require(frozen.frame == 7 && !frozen.resumeAtSix && (frozen.flags & 2) && frozen.textureFrame == 0,
                "Original player ice automatically resumes its transform at frame six and wraps its independent texture clock.");
            frozen.flags |= 0x10;
            ice.advance(frozen, true);
            require(ice.transform->frame == 7 && frozen.frame == 7, "Paused player ice publishes but does not advance animation frames.");
            frozen.flags &= 0xef;
            ice.advance(frozen, false);
            require(frozen.flags == 14 && !frozen.emitters[0] && !frozen.emitters[1] && frozen.resumeAtSix &&
                iceEmitters.emitters.at(0).flags == 0x1c && iceEmitters.emitters.at(1).flags == 0x1c,
                "Player ice completion releases both emitters before resetting their ownership pointers.");
            require(iceFactory.create(0xb, position) == &frozen && (frozen.flags & 1) &&
                !frozen.emitters[0] && !frozen.emitters[1],
                "Player ice still activates when both emitter allocations fail.");
            particles::Emitter retained;
            retained.flags = 0xe;
            frozen.emitters[0] = &retained;
            frozen.reset();
            require(retained.flags == 0xe && !frozen.emitters[0],
                "Direct ice-owner reset clears pointers without releasing them; creation and completion own the releases.");
            for (unsigned variant = 0; variant < 2; ++variant) {
                particles::EmitterPool barrierEmitters(1, 0, 0);
                modeleffects::BarrierPool barrier(variant, barrierEmitters);
                barrier.textureAnimation = std::make_shared<j3dmodel::Animation>();
                barrier.textureAnimation->frameCount = 30;
                auto& wall = static_cast<modeleffects::BarrierOwner&>(*barrier.owners[0]);
                wall.model = std::make_unique<j3dmodel::ModelInstance>(std::make_shared<j3dmodel::ModelData>(), 0x10000);
                wall.model->matrix = matrix;
                wall.particle = archive.get(0x4b485f45, "parameters");
                wall.texture = archive.get(0x4b485f45, "texture");
                wall.flags = 0x45;
                wall.speed = 9;
                wall.animationFrame = 29;
                barrier.advance(wall, true);
                require(wall.speed == 10 && wall.animationFrame == 0 && !(wall.flags & 0x80) && barrierEmitters.attempts == 0,
                    "Barrier acceleration reaches ten without beginning its reverse phase.");
                wall.animationFrame = 29;
                barrier.advance(wall, true);
                require(wall.speed == -0.5f && wall.animationFrame == 20 && (wall.flags & 0x80) &&
                    barrierEmitters.emitters.at(0).position == std::array<float, 3>{12, 214, 56} &&
                    barrierEmitters.emitters.at(0).parameters.field14C == 90,
                    "Barrier's next wrap reverses playback and emits at the original raised position with lifetime ninety.");
                wall.animationFrame = 5.5f;
                barrier.advance(wall, true);
                require(wall.animationFrame == 5 && (wall.flags & 1), "Barrier reverse release threshold excludes frame five.");
                barrier.advance(wall, true);
                require(wall.flags == 14 && wall.animationFrame == 0 && wall.speed == 1 && wall.particle,
                    "Barrier release resets its fractional clock and speed while retaining cached particle resources.");
                barrier.advance(wall, false);
                require(wall.flags == 6, "Inactive barrier clears deferred release on its next update.");
                wall.flags = 0x11;
                barrier.advance(wall, true);
                require(wall.animationFrame == 0, "Barrier pause suppresses its clock independently of running.");
                wall.flags = 1;
                barrier.advance(wall, true);
                require(wall.animationFrame == 1, "Barrier clock does not require the generic running bit.");
                require(barrier.available() == &wall && wall.flags == 6 && wall.animationFrame == 0,
                    "Barrier allocation resets and reuses its active singleton.");
            }
        }
        {
            modeleffects::IciclePool icicles(2, 1, 2, 0x21);
            icicles.transform = std::make_shared<j3dmodel::Animation>();
            icicles.transform->frameCount = 3;
            auto& owner = dynamic_cast<modeleffects::IcicleOwner&>(*icicles.owners[0]);
            owner.textureAnimation = std::make_shared<j3dmodel::Animation>();
            owner.textureAnimation->frameCount = 2;
            owner.flags = 1;
            icicles.advance(owner, true);
            require(owner.textureFrame == 1 && owner.frame == 0 && owner.elapsed == 0,
                "Icicle texture advances while its transform and lifetime are stopped.");
            owner.flags = 0x13;
            icicles.advance(owner, true);
            require(owner.textureAnimation->frame == 1 && owner.textureFrame == 1 && owner.frame == 0,
                "Paused icicle publishes its texture frame without advancing any clock.");
            owner.flags = 3;
            icicles.advance(owner, true);
            require(owner.textureFrame == 0 && owner.frame == 1 && owner.elapsed == 1,
                "Running icicle advances its independent texture, transform and lifetime counters.");
            icicles.advance(owner, true);
            require(owner.flags == 14 && owner.frame == 0 && owner.textureFrame == 0 && owner.elapsed == 0 &&
                icicles.available() == icicles.owners[1].get(), "Lifetime equality resets the owner and delays its reuse with the release bit.");
            icicles.advance(owner, false);
            require(icicles.available() == &owner, "Inactive update returns the released icicle to first-free allocation.");
        }
        {
            modeleffects::BridgePool bridge;
            bridge.colorAnimation = std::make_shared<j3dmodel::Animation>();
            bridge.textureAnimation = std::make_shared<j3dmodel::Animation>();
            bridge.colorAnimation->frameCount = 3;
            bridge.textureAnimation->frameCount = 2;
            auto* owner = bridge.available();
            owner->flags = 1;
            owner->frame = 1;
            owner->setFrame(91);
            require(owner->frame == 1, "The original bridge frame setter is an empty virtual hook.");
            bridge.colorAnimation->frame = 1.75f;
            bridge.advance(*owner, true);
            require(bridge.textureAnimation->frame == 1 && owner->frame == 0 && bridge.colorAnimation->frame == 2,
                "Bridge update publishes the prior texture frame, wraps its clock and truncates color advancement without requiring the running flag.");
            bridge.advance(*owner, true);
            bridge.advance(*owner, true);
            require(bridge.colorAnimation->frame == 3, "Bridge color animation stops at its terminal frame.");
            owner->flags |= 0x10;
            const auto frame = owner->frame;
            bridge.advance(*owner, true);
            require(owner->frame == frame, "Bridge pause suppresses both animation clocks.");
            owner->flags = 9;
            bridge.advance(*owner, false);
            require(owner->flags == 9, "Invisible bridge update retains the release bit unlike the generic effect pool.");
            require(bridge.available() == owner && owner->frame == 0 && owner->flags == 6 && bridge.colorAnimation->frame == 3,
                "Bridge allocation reuses its sole active owner and retains shared color animation state.");
        }
        {
            modeleffects::IceJailPool ice;
            ice.colorAnimation = std::make_shared<j3dmodel::Animation>();
            ice.textureAnimation = std::make_shared<j3dmodel::Animation>();
            ice.textureAnimation->frameCount = 2;
            auto* owner = ice.available();
            owner->flags = 0x11;
            owner->frame = 1;
            owner->setFrame(2.75);
            ice.advance(*owner, true);
            require(owner->frame == 1 && ice.textureAnimation->frame == 1 && ice.colorAnimation->frame == 2.75f,
                "Paused ice jail still publishes both animation frames and preserves fractional color values.");
            owner->flags = 1;
            ice.advance(*owner, true);
            require(owner->frame == 0 && ice.colorAnimation->frame == 2.75f,
                "Ice jail advances only the texture clock and ignores the generic running bit.");
            owner->flags = 9;
            ice.advance(*owner, false);
            require(owner->flags == 1, "Invisible ice jail clears its release bit.");
            require(ice.available() == owner && owner->frame == 0 && owner->flags == 6 &&
                dynamic_cast<modeleffects::IceJailOwner&>(*owner).colorFrame == 0,
                "Ice jail reuses its active singleton and resets both owner clocks.");
        }
        lights.beginFrame();
        require(lights.count() == 1 && lights.entry(0).light.color == std::array<std::uint8_t, 4>{} &&
            lights.entry(0).light.referenceDistance == 10, "Frame reset reserves the original transparent primary light.");
        modellighting::RoomLight light;
        light.position = {1, 2, 3};
        for (unsigned i = 0; i < 5; ++i) require(lights.add(0, light), "Original light pool fills six slots.");
        require(!lights.add(0, light) && lights.count() == 6 && lights.last() == 5,
            "Equal-priority overflow retains the original selection and pool.");
        require(lights.add(2, light) && lights.last() == 1 && lights.entry(0).priority == 0,
            "Light replacement skips slot zero and chooses the first lowest-priority entry.");
        require(lights.add(1, light) && lights.last() == 2, "Subsequent replacement preserves higher-priority lights.");
        lights.disabled = 1;
        lights.beginFrame();
        require(lights.count() == 0 && !lights.setPrimary(3, light), "Disabled frame reset does not allocate a primary light.");
        lights.disabled = 2;
        require(lights.setPrimary(3, light) && lights.count() == 0 && lights.last() == 2,
            "Only disable value one blocks writes; primary replacement changes neither count nor selection.");
        lights.beginFrame();
        require(lights.count() == 1 && lights.lights()[0].direction == std::array<float, 3>{0, 0, 1},
            "Point light records reset direction before reaching the original lighting consumer.");
        {
            std::array<float, 3> ownerPosition{1, 2, 3}, firstPosition{4, 5, 6}, secondPosition{7, 8, 9};
            PositionalSound first, stopped, second, unrelated;
            first.properties = second.properties = 0x8000;
            first.ownerPosition = stopped.ownerPosition = second.ownerPosition = &ownerPosition;
            unrelated.ownerPosition = &firstPosition;
            first.position = &firstPosition;
            second.position = &secondPosition;
            first.next = &stopped;
            stopped.next = &second;
            jaudio::PositionalSound* firstHandle = &first;
            jaudio::PositionalSound* secondHandle = &second;
            first.binding = &firstHandle;
            second.binding = &secondHandle;
            jaudio::RetainedPosition retained, spare;
            retained.next = &spare;
            spare.previous = &retained;
            jaudio::PositionOwners positions;
            positions.free = &retained;
            positions.categories = {&first, &unrelated};
            positions.release(&ownerPosition);
            require(positions.active == &retained && positions.free == &spare && retained.remaining == 600 &&
                retained.sound == &second && retained.position == secondPosition &&
                first.position == second.position && first.binding == second.binding &&
                !firstHandle && !secondHandle && !first.ownerPosition && !second.ownerPosition &&
                stopped.stops == 1 && unrelated.stops == 0,
                "Actor sound release retains one shared point across matching sounds and saves next before stopping.");
            for (unsigned i = 0; i < 599; ++i) positions.update();
            require(retained.remaining == 1 && !second.stops, "Retained position lasts exactly 600 original audio updates.");
            positions.update();
            require(second.stops == 1 && !positions.active && positions.free == &retained &&
                retained.next == &spare && spare.previous == &retained,
                "Retained expiry stops its last bound sound before returning the point to the free list.");
            PositionalSound exhausted;
            exhausted.properties = 0x8000;
            exhausted.ownerPosition = &ownerPosition;
            positions.free = nullptr;
            positions.categories = {&exhausted};
            positions.release(&ownerPosition);
            require(exhausted.stops == 1 && !positions.active, "Exhausted retained storage stops persistent sounds.");
        }
        {
            auto data = std::make_shared<std::vector<std::uint8_t>>(512);
            auto put = [&](unsigned offset, std::uint32_t value) {
                for (unsigned i = 0; i < 4; ++i) (*data)[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
            };
            for (unsigned i = 0; i < data->size(); i += 4) put(i, 0x10203000 + i);
            const unsigned base = 16;
            put(base, 0xff2fc0);
            (*data)[base + 0x2c] = 2;
            (*data)[base + 0x48] = 1;
            (*data)[base + 0x54] = 3;
            (*data)[base + 0x118] = 0x11;
            (*data)[base + 0x119] = 0x22;
            (*data)[base + 0x11a] = 0x33;
            particles::Parameters parameters;
            parameters.load(data, base);
            require(parameters.curves[0].count == 2 && parameters.curves[1].count == 1 && parameters.curves[2].count == 3 &&
                parameters.curves[0].value(false, 1) == 0x10203044 && parameters.curves[2].value(true, 2) == 0x1020307c &&
                parameters.fields10C[0] == 0x10203080 && parameters.fields168[0] == 0x102030c8 &&
                parameters.fields150[0] == 0x10203144 && parameters.fields1C8 == std::array<std::uint8_t, 3>{0x11, 0x33, 0x22},
                "Original particle parameters follow counted curve pairs and all optional blocks in declaration order.");
            const auto retained = parameters.fields168;
            const auto curveSource = parameters.curves[0].source;
            auto plain = std::make_shared<std::vector<std::uint8_t>>(0x88);
            parameters.load(plain, 0);
            require(parameters.flags == 0 && parameters.fieldF4 == 0 && parameters.fields168 == retained &&
                parameters.curves[0].source == curveSource,
                "Particle reload preserves fields and borrowed curve storage omitted by the new flags.");
            plain->resize(0x87);
            bool rejected = false;
            try { parameters.load(plain, 0); } catch (const std::out_of_range&) { rejected = true; }
            require(!rejected, "Original particle initialization does not read the final padding byte.");
            plain->resize(0x86);
            try { parameters.load(plain, 0); } catch (const std::out_of_range&) { rejected = true; }
            require(rejected, "Truncated particle parameter fields are rejected.");
        }
        {
            particles::Pool<particles::Node> pool(1024);
            particles::Pool<particles::ShortNode> shortPool(1024);
            particles::Ring<particles::Node> active;
            require(pool.free.count == 1024 && pool.free.sentinel.next == &pool.at(0) &&
                pool.free.sentinel.previous == &pool.at(1023) && shortPool.free.count == 1024,
                "Original particle pools append constructed nodes in index order around their sentinels.");
            auto* first = pool.free.take();
            auto* second = pool.free.take();
            active.append(*first);
            active.append(*second);
            require(first == &pool.at(0) && second == &pool.at(1) && active.count == 2 &&
                first->next == second && second->previous == first && pool.free.count == 1022,
                "Particle allocation transfers the free-list front to the active-list tail with stable object identity.");
            first->fields24 = {1, 2, 3};
            first->fields35 = {4, 5, 6};
            first->fields59 = {7, 8, 9};
            first->fields0.fill(17);
            first->field48 = 9;
            first->reset();
            require(first->fields0 == std::array<float, 9>{} && first->field48 == 1 &&
                first->fields24 == std::array<std::uint32_t, 3>{1, 2, 3} &&
                first->fields35 == std::array<std::uint8_t, 3>{4, 5, 6} &&
                first->fields59 == std::array<std::uint8_t, 3>{7, 8, 9} && first->next == second,
                "Particle reset preserves original untouched fields and intrusive links.");
            pool.free.append(*active.take());
            require(pool.free.sentinel.previous == first && active.sentinel.next == second,
                "Returned particles retain original tail-insertion reuse order.");
            particles::Pool<particles::ShortNode> empty(0);
            require(!empty.free.take() && empty.free.current == &empty.free.sentinel,
                "Empty particle allocation retains its previous current pointer.");
        }
        {
            particles::EmitterPool pool(2, 1024, 1024);
            auto source = std::make_shared<std::vector<std::uint8_t>>(0xc0);
            (*source)[5] = 0x30;
            (*source)[16] = 2;
            (*source)[18] = 7;
            (*source)[19] = 2;
            auto texture = std::make_shared<std::vector<std::uint8_t>>(1);
            auto alternate = std::make_shared<std::vector<std::uint8_t>>(2);
            require(!pool.create({}, texture) && pool.attempts == 0,
                "Missing particle resources do not consume an allocation attempt.");
            auto* emitter = pool.create(source, texture, alternate);
            require(emitter == &pool.emitters.at(0) && emitter->flags == 0xe &&
                emitter->draw == &emitter->extendedDraw && emitter->draw->arrays == std::array<unsigned, 3>{20, 28, 36} &&
                emitter->draw->fields34 == std::array<std::uint8_t, 2>{1, 1} && emitter->draw->fields18[2] == 1 &&
                emitter->draw->alternateTexture == alternate && emitter->nodePool == &pool.nodes.free &&
                pool.active[0].count == 1 && emitter->empty(),
                "Emitter creation selects the original extended draw layout and shared particle pools.");
            emitter->release();
            require(emitter->flags == 0x1c && !emitter->inactive(), "Emitter release preserves deferred particle retirement flags.");
            emitter->stop();
            require(emitter->flags == 0x3c, "Emitter stop retains existing status bits.");
            (*source)[6] = 1;
            (*source)[8] = 0;
            (*source)[11] = 3;
            pool.destination = &pool.active[1];
            auto* second = pool.create(source, texture);
            require(second == &pool.emitters.at(1) && second->draw == &second->ordinaryDraw &&
                second->draw->arrays[0] == 12 && second->draw->arrays[1] == 24 &&
                second->draw->alternateTexture == texture && pool.active[1].count == 1,
                "Ordinary emitter creation respects the selected active list and primary texture fallback.");
            require(!pool.create(source, texture) && pool.attempts == 3,
                "A full emitter pool still records valid resource allocation attempts.");
            struct Simulation final : particles::Simulation {
                unsigned callbacks = 0, emissions = 0, advances = 0;
                void callback(particles::Emitter& owner) override { ++callbacks; owner.flags = 0x100; }
                void emit(particles::Emitter&) override { ++emissions; }
                void advance(particles::Emitter&) override { ++advances; }
            } simulation;
            pool.enterAlternate();
            auto* node = pool.nodes.free.take();
            auto* shortNode = pool.shortNodes.free.take();
            node->fields0.fill(5);
            shortNode->fields0.fill(6);
            shortNode->fields1A.fill(7);
            second->nodes.append(*node);
            second->shortNodes.append(*shortNode);
            second->flags = 7;
            struct Callback : particles::EmitterCallback {
                void advance(particles::Emitter&) override {}
            } callback;
            second->fieldC4 = &callback;
            pool.update(simulation);
            require(simulation.callbacks == 1 && simulation.emissions == 1 && simulation.advances == 1 &&
                second->flags == 0x100 && pool.attemptHistory[0] == 3 && pool.attempts == 0,
                "Emitter callbacks do not change the captured flags used for the current update.");
            pool.update(simulation);
            require(simulation.callbacks == 1 && simulation.advances == 1,
                "Paused emitters skip callback, emission, simulation and retirement.");
            pool.leaveAlternate();
            require(pool.active[1].count == 0 && pool.active[2].count == 1 && pool.emitters.free.count == 0 &&
                second->inactive() && pool.nodes.free.count == 1024 && pool.shortNodes.free.count == 1024 &&
                node->fields0 == std::array<float, 9>{} && shortNode->fields0 == std::array<float, 6>{} &&
                shortNode->fields1A == std::array<std::uint8_t, 6>{7, 7, 7, 7, 7, 7},
                "Leaving the alternate list returns particles and defers emitter reuse while preserving unwritten bytes.");
            pool.update(simulation);
            require(pool.emitters.free.count == 1 && pool.active[2].count == 1 && emitter->inactive(),
                "The following update recycles prior retirees before retiring the current active emitter.");
            pool.update(simulation);
            require(pool.emitters.free.count == 2 && pool.emitters.free.sentinel.next == second &&
                pool.emitters.free.sentinel.previous == emitter,
                "Deferred emitter recycling retains the original retirement order.");
        }
        {
            modeleffects::Definition definition;
            definition.capacity = 2;
            definition.counts = {2, 1, 1};
            definition.lifetime = 3;
            modeleffects::Pool pool(definition);
            for (auto& group : pool.animations) for (auto& animation : group) {
                animation = std::make_shared<j3dmodel::Animation>();
                animation->frameCount = 2;
            }
            auto* first = pool.available();
            require(first == pool.owners[0].get() && first->flags == 6 && first->frames[0].size() == 2,
                "Model-effect construction owns the declared frame arrays and starts available.");
            first->flags |= 1;
            require(pool.available() == pool.owners[1].get(), "Model-effect acquisition scans active status in original index order.");
            pool.advance(*first, true);
            require(first->elapsed == 1 && first->frames[0][0] == 1, "Model effects advance lifetime and animation clocks together.");
            first->flags |= 0x10;
            pool.advance(*first, true);
            require(first->elapsed == 1, "Paused model effects retain their clocks.");
            first->flags &= 0xef;
            pool.advance(*first, false);
            require(first->elapsed == 1, "Invisible-room model effects retain their clocks.");
            pool.advance(*first, true);
            require(first->elapsed == 2 && first->frames[0][0] == 0, "Model-effect frames wrap at the original signed animation end.");
            pool.advance(*first, true);
            require(first->flags == 14 && first->elapsed == 0 && first->attachmentState() == 1 &&
                pool.available() == pool.owners[1].get(), "Lifetime release resets state but defers slot reuse.");
            pool.advance(*first, false);
            require(first->flags == 6 && pool.available() == first, "The next pool update clears the model-effect reuse gate.");
            first->setFrame(-2.75);
            require(first->frame == -2 && first->frames[2][0] == -2, "Explicit model-effect frame changes truncate toward zero across all clocks.");
        }
        {
            modeleffects::SharedPool pool;
            pool.textureAnimation = std::make_shared<j3dmodel::Animation>();
            pool.colorAnimation = std::make_shared<j3dmodel::Animation>();
            pool.textureAnimation->frameCount = 4;
            pool.colorAnimation->frameCount = 6;
            auto& owner = dynamic_cast<modeleffects::SharedOwner&>(*pool.available());
            owner.flags = 7;
            owner.frame = 3;
            owner.colorFrame = 4;
            pool.advance(owner, true);
            require(pool.textureAnimation->frame == 3 && pool.colorAnimation->frame == 4 && owner.frame == 0 && owner.colorFrame == 5,
                "Shared effect animation resources receive the pre-increment owner clocks.");
            owner.flags |= 0x10;
            pool.advance(owner, true);
            require(pool.textureAnimation->frame == 0 && pool.colorAnimation->frame == 5 && owner.colorFrame == 5,
                "Paused shared effects still publish their retained animation clocks.");
            owner.release();
            require(owner.flags == 14 && owner.frame == 0 && owner.colorFrame == 0 && pool.available() == &owner,
                "Shared model effects intentionally permit immediate reuse despite the release bit.");
        }
        {
            characterevent::Observer defaults;
            for (unsigned message = 0; message < 40; ++message) {
                characterevent::Event event(message);
                require(event.fields == std::array<std::uint32_t, 6>{} &&
                    defaults.receive(event) == (message == 0 ? 1u : 0u),
                    "Original event construction clears payload words; default observer accepts only message zero.");
            }
            struct ElementObserver : characterevent::Observer {
                characterevent::Event* last = nullptr;
                std::uint32_t sprayedWithFire(characterevent::Event& event) override { last = &event; return 0x81; }
                std::uint32_t sprayedWithIce(characterevent::Event& event) override { last = &event; return 0x42; }
                std::uint32_t sprayedWithWater(characterevent::Event& event) override { last = &event; return 0x24; }
                std::uint32_t resources(characterevent::Event& event) override {
                    last = &event;
                    event.types->at(event.fields[0]) = 1;
                    return 0xff;
                }
            } observer;
            const std::array<std::uint32_t, 3> results{0x81, 0x42, 0x24};
            for (unsigned i = 0; i < 3; ++i) {
                characterevent::Event spray(13 + i), ball(16 + i);
                ball.fields[2] = 0x12345678;
                require(observer.receive(spray) == results[i] && observer.last == &spray &&
                    observer.receive(ball) == results[i] && observer.last == &ball &&
                    ball.message == 16 + i && ball.fields[2] == 0x12345678,
                    "Original ball callbacks delegate virtually to spray handlers without replacing the event or its result.");
            }
            std::array<std::uint8_t, 381> mask{};
            characterevent::Event event(29);
            event.types = &mask;
            event.fields[0] = 380;
            require(observer.receive(event) == 0xff && mask[380] == 1 && observer.last == &event,
                "The resource observer receives the original mutable payload and mask identity.");
        }
        std::cout << "Character ownership checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
