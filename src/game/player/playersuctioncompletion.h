#pragma once

#include "game/enemies/itemsuctioncontroller.h"
#include "game/player/playercaptureflags.h"
#include "game/actors/actorregistry.h"

#include <functional>

namespace playerevents {
    class SuctionSounds {
    public:
        virtual ~SuctionSounds() = default;
        virtual void actor(unsigned sound, const itemsuction::Vector& position, unsigned parameter) = 0;
        virtual void weapon(unsigned sound, const itemsuction::Vector& position) = 0;
    };

    struct SuctionEnvironment {
        const actormodel::Catalog& catalog;
        const charactercreation::Properties& properties;
        gameflags::State& flags;
        SuctionSounds& sounds;
        const std::int32_t& elementDuration;
    };

    bool completeSuction(playeractor::Player& player, const itemsuction::Completion& payload, const SuctionEnvironment& environment);

    class SuctionEvents final : public itemsuction::Events {
    public:
        SuctionEvents(playeractor::Player& player, const actors::Registry& actors, const SuctionEnvironment& environment,
            std::function<void(actors::MoveActor&)> collector);
        void attraction(unsigned actor) override;
        void completion(const itemsuction::Completion& payload) override;
        void collect(actors::MoveActor& actor) override;

    private:
        playeractor::Player& player;
        const actors::Registry& actors;
        const SuctionEnvironment& environment;
        std::function<void(actors::MoveActor&)> collector;
    };
}
