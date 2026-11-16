#pragma once

#include "game/player/playervacuumscan.h"
#include "game/player/playeractor.h"
#include "game/player/playereventdispatch.h"
#include "game/player/playerliferecovery.h"
#include "game/player/playervacuumcomponent.h"
#include <functional>
#include <vector>

namespace vacuumcapture {
    struct PlayerFrameServices {
        Vacuum& vacuum;
        const playercomponents::VacuumGlobals& globals;
        // 800B9BB8 -> 80086730: controller rumble pattern index.
        std::function<void(unsigned)> rumble;
        // 8018ACF8 (mode 8 while the vacuum is blocked).
        std::function<void(float, const Vector&, unsigned)> loopSound;
        // 80016C98(7, position, colour, a, b): the dynamic light at the nozzle.
        std::function<void(const Vector&, const std::array<std::uint8_t, 4>&, int, int)> beamLight;
    };

    // 800B3BAC up to its energy tail (native `playeractor::EnergyFrame::run` takes the returned flag).
    class PlayerFrame {
    public:
        explicit PlayerFrame(const std::vector<std::uint8_t>& executable);
        // Returns whether the vacuum was updated this frame (bVar3).
        bool run(playeractor::Player& player, PlayerFrameServices& services) const;
        // 800B99D8: nozzle-reach scale from the player's movement and aim state.
        float reachScale(const playeractor::State& state) const;

    private:
        dooranimation::PoseTables trigonometry_;
        float one_, half_, minusOne_, units_, five_, hundredEighty_, threeSixty_, tintHold_, tintTake_, nozzleLift_;
        std::int32_t windowA_, windowB_, windowC_;
        std::uint8_t alpha_;
        struct Tint { std::uint8_t color[3]; std::int32_t first, second; };
        std::array<Tint, 5> tints_; // modes 1..4 and the idle row (index 0)
        void applyTint(Vacuum& vacuum, unsigned mode, const playermodels::VacuumVolume& volume,
            const PlayerFrameServices& services, const playermodels::VacuumModel* model) const;
    };

    struct ReactionServices {
        Controller& controller;
        playermodels::VacuumEntries& entries;
        const playerevents::Dispatcher& events;
        playerevents::LifeEnvironment& life;
        unsigned playerIndex = 0;
        std::function<void(unsigned)> rumble;                                   // 800B9BB8
        std::function<void(unsigned, const Vector*)> sound;                     // 80189F78
        std::function<Vector(const actors::MoveActor&, unsigned)> jointPosition; // 80097B9C
        std::function<unsigned(const actors::MoveActor&)> attachmentJoint;       // 800E9548
        std::function<int(unsigned)> typeGroup;                                  // 8006A3D8 + 0x14
        std::function<std::uint32_t(const actors::MoveActor&)> slotHandle;       // 80097FAC
        std::function<void(const Vector&)> releaseTrail;                         // 8008907C
        std::function<bool()> doubledDamage;                                     // DAT_804D80C0
    };

    // The remainder of 800B41E0 after `EnergyFrame::beginCaptureReaction`: the player's reaction to its capture
    // record (look target, actions, sounds, rumble) and the per-frame `setTracking`.
    class CaptureReaction {
    public:
        explicit CaptureReaction(const std::vector<std::uint8_t>& executable);
        void run(playeractor::Player& player, std::uint8_t previousReaction, ReactionServices& services) const;
        // 800B9848.
        float nozzleRange(const playeractor::State& state, const playeractions::Reactions& reactions) const;

    private:
        float lookDistance_, lookFrames_, weakStrength_, strongStrength_, trailHeight_;
        float rangeBase_, rangeIdle_, rangeHeld_, rangeAir_, rangeBoost_, rangeCarry_, rangeSlow_, rangeSlowAlt_, rangeFast_;
        float half_;
        std::int32_t drainPeriod_;
        float rumbleLimit_;
    };
}