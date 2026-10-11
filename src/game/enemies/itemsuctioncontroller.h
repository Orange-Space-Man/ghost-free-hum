#pragma once

#include "game/enemies/itemsuctionforce.h"
#include "game/actors/moveactor.h"
#include "game/world/roomtraversalstate.h"
#include "game/effects/particleeffectfactory.h"
#include "game/events/runtimeflags.h"

namespace itemsuction {
    struct Completion {
        std::uint32_t kind = 0, amount = 0, type = 0;
        std::int32_t collectionBit = -1;
        std::uint32_t delayEnabled = 0, delay = 10;
    };

    class Events {
    public:
        virtual ~Events() = default;
        virtual void attraction(unsigned actor) = 0;
        virtual void completion(const Completion& payload) = 0;
        virtual void collect(actors::MoveActor& actor) = 0;
    };

    struct Environment {
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        const actormodel::InitialPose& pose;
        const particles::SuctionContext& context;
        Force& force;
        particles::EffectFactory& effects;
        gameflags::State& flags;
        Events& events;
    };

    class Controller {
    public:
        Controller(Environment& environment, std::uint8_t retainedRemaining);
        void initialize(const actors::MoveActor& actor, std::uint8_t countdown, std::uint8_t durationBase);
        bool update(actors::MoveActor& actor, double heightOffset, double arrivalRadius, const Vector& unwrittenDirection);
        bool updateVelocity(actors::MoveActor& actor, Vector& velocity, const Vector& unwrittenDirection);
        void complete(actors::MoveActor& actor);
        void updateScale(actors::MoveActor& actor);
        void recoverScale(actors::MoveActor& actor);
        void orient(actors::MoveActor& actor, const Vector& velocity);
        std::int32_t completionKind = 0;
        std::uint32_t completionValue = 0;
        std::uint8_t emitEffects = 1;
        Vector acceleration{}, minimum{}, maximum{};
        float scale = 1, originalScale = 1, animationRate = 1;
        std::uint8_t remaining, duration = 255, field42 = 0, attracting = 0, countdown = 0;
        float minimumScale = 0.3f, scaleDistance = 200, heightMargin = 0;
        Parameters parameters;

    private:
        Environment& environment;
        bool visible(const actors::MoveActor& actor) const;
        void begin(actors::MoveActor& actor);
        void recover(actors::MoveActor& actor);
        void restrictHeight(const actors::MoveActor& actor, Vector& velocity) const;
    };
}
