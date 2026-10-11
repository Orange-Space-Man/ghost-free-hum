#pragma once

#include "game/actors/charactercreationstate.h"
#include "game/actors/characteractorlinks.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/actors/characterownerevents.h"
#include "game/actors/characterownermotion.h"
#include "game/actors/characterownervacuum.h"

namespace particles { class Scene; }
namespace players { class Manager; }

namespace characterowner {
    struct UpdateServices {
        const actormodel::Catalog& models;
        const actormodel::Resources& resources;
        const actormodel::InitialPose& pose;
        const enemytypes::Manager& parameters;
        const enemycreation::Catalog& creation;
        const mapcollision::State& collision;
        const actors::Registry& actors;
        players::Manager& players;
        enemies::Runtime& runtime;
        modeleffects::Factory& modelEffects;
        particles::Scene& particles;
        std::function<void(unsigned, const std::array<float, 3>&, unsigned)> sound;
        const std::array<float, 3>* originalMotionScratch = nullptr;
    };

    class CreationRuntime : public Runtime {
    public:
        CreationRuntime(characterownership::Resources& resources, const charactercreation::Properties& properties,
            playerrecord::Record& player, roomlighting::Pool& lights, jaudio::PositionOwners& sounds);
        void bind(enemies::Manager& manager, const enemystrategy::Factory& factory);
        void bindCapture(CaptureServices* services);
        void bindItemCreation(characteritems::Creation* items);
        void bindUpdates(UpdateServices* services);
        void update(Owner& owner) override;
        characterownership::Resources& resources() override;
        characterownership::ActorLinks& links(enemies::Actor& actor) override;
        void resetActor(enemies::Actor& actor, const jmp::ToolDataRef& source) override;
        void clearInactiveFlags(enemies::Actor& actor) override;
        void initializeActor(enemies::Actor& actor, unsigned type) override;
        std::unique_ptr<enemystrategy::Strategy> constructStrategy(std::uint32_t constructor, Owner& owner,
            unsigned capacity) override;
        void updateEffects(Owner& owner) override;
        void creationEvent(Owner& owner) override;
        std::uint32_t color(unsigned type) override;
        void writeColor(enemies::Actor& actor, std::uint32_t color) override;
        std::uint32_t event(Owner& owner, characterevent::Event& event) override;

    private:
        characterownership::Resources& effects;
        const charactercreation::Properties& properties;
        playerrecord::Record& player;
        roomlighting::Pool& lights;
        jaudio::PositionOwners& sounds;
        enemies::Manager* manager = nullptr;
        const enemystrategy::Factory* factory = nullptr;
        CaptureServices* capture = nullptr;
        characteritems::Creation* items = nullptr;
        UpdateServices* updates = nullptr;
        std::array<std::unique_ptr<characterownership::Links>, 128> actorLinks;
        std::array<enemies::Actor*, 128> linkedActors{};
    };
}
