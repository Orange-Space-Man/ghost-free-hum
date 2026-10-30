#pragma once

#include "playersheetmotion.h"
#include "game/player/playereventpayload.h"
#include "game/player/playervacuummodel.h"

namespace playersheets {
    struct SuctionOwner {
        const playermodels::VacuumVolume* volume;
        const playerscanner::Vector& position;
        const playerscanner::Vector& direction;
    };

    class SheetServices {
    public:
        virtual ~SheetServices() = default;
        virtual void sound(unsigned id, const playerscanner::Vector& position) = 0;
        virtual void completion(const playerevents::Payload& payload) = 0;
        virtual void completionEffect(unsigned channel, const playerscanner::Vector& position,
            const playerscanner::Vector& direction) = 0;
        virtual void impact(float radius, const playerscanner::Vector& position, unsigned count) = 0;
        virtual void secondaryTexture() = 0;
        virtual void rustle(unsigned id, const playerscanner::Vector& position, float volume) = 0;
        virtual bool requestPull(Grid& sheet, const playerscanner::Vector& center, float range, float elasticity) = 0;
        virtual bool elasticReaction(const playerscanner::Vector& center, unsigned reaction, unsigned action) = 0;
        virtual void releasePull(Grid& sheet) = 0;
    };

    class Suction {
    public:
        Suction(Grid& grid, Motion& motion, const std::vector<std::uint8_t>& executable);
        void attract(const SuctionOwner* owner, SheetServices& services);
        void requestRetraction(bool hasOwner);
        void updateFeedback(bool hasPlayer, bool hasVolume, bool playerHolding, bool playerPulling, SheetServices& services);
        void collide(SheetServices& services, bool hasSecondaryTexture);
        void updateSound(SheetServices& services);
        bool completed = false, impactReady = false, releaseStarted = false;
        playerscanner::Vector captureCenter{};
        float previousSpeed, soundDelay;
        float pullFrames = 0;

    private:
        Grid& grid;
        Motion& motion;
        float queryRadius, attachedRadiusSquared, detachedRadiusSquared, attachedForce, detachedForce;
        float impactSpeed, impactRadius, soundSpeed, soundScale, pullRange;
    };
}
