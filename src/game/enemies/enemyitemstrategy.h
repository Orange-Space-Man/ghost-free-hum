#pragma once

#include "game/enemies/enemyitemappearance.h"
#include "game/enemies/enemyitemphysics.h"
#include "game/enemies/enemyitemlifetime.h"
#include "game/player/playersuctioncompletion.h"
#include "game/player/playeritemcollection.h"
#include "game/scene/gamearchive.h"

namespace playerevents { struct LifeEnvironment; struct ShrinkEnvironment; }
namespace actortrail { class Pool; }
namespace gameplaycamera { struct CameraOwner; }
namespace jaudio { struct PositionOwners; }

namespace enemystrategy {
    class Factory;
    class Catalog;

    class ItemSounds {
    public:
        virtual ~ItemSounds() = default;
        virtual void play(unsigned sound, const itemsuction::Vector& position, unsigned parameter) = 0;
    };

    struct ItemEnvironment {
        const gamearchive::Archive& common;
        const enemytypes::Catalog& types;
        const Catalog& strategies;
        const mapcollision::State& collision;
        const actors::RotatingSurfaces& surfaces;
        const actorcollision::Catalog& volumes;
        playerevents::ItemEnvironment& collection;
        playerevents::SuctionEnvironment& feedback;
        itemsuction::Environment& suction;
        ItemSounds& sounds;
        std::function<itemsuction::Vector()> unwrittenDirection;
        std::uint8_t retainedRemaining;
        const std::uint8_t& durationBase;
        gameparameters::Parameters keyParameters;
        std::function<void(unsigned)> globalSound;
        const std::uint32_t* mansionMode = nullptr;
        playerevents::LifeEnvironment* life = nullptr;
        gameparameters::Parameters negativeParameters;
        playerevents::ShrinkEnvironment* shrink = nullptr;
        actortrail::Pool* trails = nullptr;
        gameparameters::Parameters bombParameters;
        gameplaycamera::CameraOwner* camera = nullptr;
        jaudio::PositionOwners* audioPositions = nullptr;
    };

    unsigned itemCondition(unsigned type);

    class FiniteItem : public Strategy, public characterevent::Observer, private itemsuction::Events {
    public:
        FiniteItem(ItemEnvironment& environment, const Factory& factory);
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t captureEnd(characterevent::Event& event) override;
        std::uint32_t playerLeftRoom(characterevent::Event& event) override;
        bool collectItem();
        bool touchingPlayer() const;
        bool canAttract() const;
        ItemPhysics physics;
        ItemLifetime lifetime;
        bool collected = false, expired = false;
        jmp::ToolDataRef itemSource;
        std::uint32_t flags = 1, groundedFrames = 0;

    protected:
        ItemEnvironment& environment;
        const Factory& factory;
        itemsuction::Environment suctionEnvironment;
        itemsuction::Controller suction;
        ItemImmediateAppearance immediate;
        ItemFadeAppearance fade;
        std::unique_ptr<Strategy> condition;
        Strategy* appearance = nullptr;
        actors::MoveActor& actor() const;
        playeractor::Player& player(unsigned index = 0) const;
        virtual void initializeItem() = 0;
        virtual void beginActive() = 0;
        virtual void updateActive() = 0;
        virtual void onCollected() = 0;
        virtual void prepareCollection(playerevents::ItemRequest& request);
        void beginFinite();
        void updateFinite(bool infinite = true);
        void bind(Strategy& strategy);
        void attraction(unsigned actor) override;
        void completion(const itemsuction::Completion& payload) override;
        void collect(actors::MoveActor& actor) override;
    };

    class Item final : public FiniteItem {
    public:
        Item(ItemEnvironment& environment, const Factory& factory);
        gameparameters::Parameters parameters;
        std::uint32_t initialRoom = 0;

    private:
        void initializeItem() override;
        void beginActive() override;
        void updateActive() override;
        void onCollected() override;
    };
}
