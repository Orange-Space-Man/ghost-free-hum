#pragma once

#include "game/enemies/enemyghostparent.h"
#include "game/enemies/enemycapturestrategy.h"
#include "game/enemies/enemylightstrategy.h"
#include "game/enemies/enemystrategycatalog.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/effects/modeleffectpool.h"
#include "game/player/playereventdispatch.h"

namespace ghostparent {
    // The owner-side objects the parent reaches through its character owner (+0x4 / +0x808 / +0x80C chains).
    struct OwnerView {
        actors::MoveActor& actor;
        characterowner::State& state;
        enemies::Manager& manager;
        std::function<void()> releaseShared;
        std::function<void(void*&)> releaseReference;
        std::function<void(unsigned)> event;
        characterownership::Ownership* ownership = nullptr;
        std::function<std::uint32_t(characterevent::Event&)> dispatch;
        std::function<void()> beginLightEffect;
    };
    OwnerView viewOf(characterowner::Owner& owner, const actors::Registry& registry);

    // Everything the original ghost parent (factory 800EED40) reaches through globals. Services the original still
    // depends on without a native equivalent are rejected explicitly.
    struct Environment {
        enemies::Runtime& runtime;
        const enemytypes::Manager& types;
        const actormodel::InitialPose& pose;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        const mapcollision::State& collision;
        const enemystrategy::Catalog& strategies;
        const enemystrategy::Factory& factory;
        const playerevents::Dispatcher& events;
        const actors::Registry& actors;
        const std::vector<std::uint8_t>& executable;
        const dooranimation::PoseTables& trigonometry;
        const enemycreation::Catalog& creation;
        const actorcollision::Catalog& shapes;
        const roomlocation::Rooms& rooms;
        const gamearchive::Archive& common;
        ghostcapture::DropServices drops;
        std::function<void(unsigned, const std::array<float, 3>&)> particle;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::function<std::uint32_t()> frame;
        std::function<actors::MoveActor&(unsigned)> player;
        std::function<bool()> reducedScale;
        std::function<OwnerView(enemies::Owner&)> view; // defaults to viewOf(characterowner::Owner)
    };

    // 800EED40: the training ghost parent. Owner-specific services are bound when the owner begins the strategy.
    std::unique_ptr<enemystrategy::Strategy> construct(const Environment& environment);
}