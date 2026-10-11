#pragma once

#include "game/actors/characterownership.h"
#include "game/enemies/enemymanager.h"
#include "game/enemies/enemystrategycatalog.h"
#include "game/enemies/enemyreplacestrategy.h"

namespace actors { class Registry; }
namespace mission { class Resources; }
namespace charactergenerator { class Generator; }
namespace mapcollision { class State; }
namespace modeleffects { class Factory; }
namespace enemytypes { class Manager; }

namespace characterowner {
    class Owner;

    struct State {
        charactergenerator::Generator* generator = nullptr;
        std::array<void*, 2> actorUsers{};
        void* field928 = nullptr;
        std::int32_t actorReferences = 0;
        std::int32_t field890 = -1;
        characterownership::Resource* field894 = nullptr;
        std::uint32_t field898 = 0, life = 0, field8a0 = 0;
        std::int32_t minimum = 0, maximum = 255;
        float value = 1, speed = 1, target = 1;
        enemies::Actor* controlActor = nullptr;
        std::int32_t field8c8 = -1;
        std::uint32_t field8cc = 0;
        std::uint8_t field8d0 = 0;
        std::uint32_t field8d4 = 0;
        std::array<float, 3> field8d8{};
        float field8e4 = 0;
        std::uint16_t field8e8 = 0;
        bool field8ea = false;
        enemystrategy::Replace embeddedStrategy;
        std::array<std::uint32_t, 7>* captureRecord = nullptr;
        std::array<bool, 3> flags904{};
        std::array<std::uint32_t, 5> fields908{};
        bool field91c = false;
        std::uint32_t field920 = 0;
        bool field924 = false;
        std::array<float, 4> fields93c{};
        std::uint16_t field94c = 0;
        std::uint32_t field950 = 0, field954 = 0, field958 = 0;
    };

    class Runtime {
    public:
        virtual ~Runtime() = default;
        virtual characterownership::Resources& resources() = 0;
        virtual characterownership::ActorLinks& links(enemies::Actor& actor) = 0;
        virtual void resetActor(enemies::Actor& actor, const jmp::ToolDataRef& source) = 0;
        virtual void clearInactiveFlags(enemies::Actor& actor) = 0;
        virtual void initializeActor(enemies::Actor& actor, unsigned type) = 0;
        virtual std::unique_ptr<enemystrategy::Strategy> constructStrategy(std::uint32_t constructor, Owner& owner,
            unsigned capacity) = 0;
        virtual void updateEffects(Owner& owner) = 0;
        virtual void creationEvent(Owner& owner) = 0;
        virtual std::uint32_t color(unsigned type) = 0;
        virtual void writeColor(enemies::Actor& actor, std::uint32_t color) = 0;
        virtual void update(Owner& owner) = 0;
        virtual std::uint32_t event(Owner& owner, characterevent::Event& event) = 0;
    };

    enum class Kind {
        Null,
        Ordinary
    };

    class Owner final : public enemies::Owner {
    public:
        Owner(Kind kind, const enemystrategy::Catalog& strategies, Runtime& runtime);
        ~Owner() override;
        void attach(enemies::Actor& actor, enemies::Manager& manager) override;
        enemies::Actor& actor() override;
        bool activate(unsigned type) override;
        void deactivate() override;
        void beginStrategy() override;
        void replaceStrategy(unsigned type) override;
        void nextStrategyState(std::uint16_t state) override;
        void update() override;
        using enemies::Owner::event;
        std::uint32_t event(characterevent::Event& event) override;
        void reset();
        void setLife(std::uint32_t value);
        void initializeLife();
        void updateStrategy();
        void updateFlagTimer();
        void updateEventTimers();
        void updateAnimationEffect();
        void updateDependentActors(const actors::Registry& actors);
        void updateSpecialAttachment(const actors::Registry& actors, const enemycreation::Catalog& creation);
        void releaseActorReference(void*& user);
        void updateReleaseTimer();
        void releaseFromManager();
        bool beginReplacement();
        Kind kind() const;
        enemies::Manager& manager();
        State& state();
        characterownership::Ownership& ownership();
        jmp::ToolData& path();
        bool loadPath(const mission::Resources& resources);
        characterownership::Resource* acquireShared(modeleffects::Factory& effects, const enemytypes::Manager& parameters,
            const mapcollision::State& collision);
        void updateProjection(modeleffects::Factory& effects, const enemytypes::Manager& parameters,
            const mapcollision::State& collision);

    private:
        Kind ownerKind;
        const enemystrategy::Catalog& strategies;
        Runtime& runtime;
        enemies::Actor* object = nullptr;
        enemies::Manager* parent = nullptr;
        characterownership::Ownership owned;
        State values;
        std::unique_ptr<jmp::ToolData> pathData;
        void initializeStrategy(unsigned type);
    };

    std::unique_ptr<enemies::Owner> construct(unsigned constructor, const enemystrategy::Catalog& strategies, Runtime& runtime);
}
