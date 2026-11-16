#pragma once

#include "game/actors/characterevent.h"
#include "game/actors/moveactor.h"
#include "game/player/playeractorstate.h"
#include "game/player/playervacuumeffects.h"
#include "game/world/dooranimationpose.h"
#include <array>
#include <functional>

namespace vacuumcapture {
    using Vector = std::array<float, 3>;

    // Services the original entries (80081244..80083DF0) reach through globals.
    struct Services {
        const actormodel::Catalog& catalog;
        const dooranimation::PoseTables& trigonometry;
        // 80097B9C: world position of a joint of the actor's model.
        std::function<Vector(const actors::MoveActor&, unsigned)> jointPosition;
        // 800E9548 (800DAC6C): attachment joint of the actor's type.
        std::function<unsigned(const actors::MoveActor&)> attachmentJoint;
        // 800DAEB0 (type parameter +0xF8) and 800DAEC8 (+0x10C).
        std::function<float(unsigned)> typeStrength;
        std::function<float(unsigned)> typeAngle;
        // 8006A36C: halfword at +4 of the actor type table entry.
        std::function<int(unsigned)> typeKind;
        // 8006A3D8 + 0x14: the actor type group (enemycreation::Catalog::group).
        std::function<int(unsigned)> typeGroup;
        // DAT_804D8BF8: global tick (float, +1.0 per frame, 0.0 at start).
        std::function<float()> tick;
        // DAT_804D80C0 (doubled damage / "ura" mode).
        std::function<bool()> reducedScale;
        // 8018A1E0 / 80189F78 sound at a position.
        std::function<void(unsigned, const Vector&)> sound;
        // 8018B4A4: capture life-drain sound sequence.
        std::function<void()> drainSound;
        // 80164F5C(3, ...) beam start particle and 80098678 dust request (id 0x1E).
        std::function<void(unsigned, const Vector&)> particle;
        std::function<void(const Vector&, unsigned)> dust;
        // 80098678-style burst of 8016696C (0 = small, 1 = large).
        std::function<void(unsigned, const Vector&, const Vector&)> burst;
        // 800C0F08 on the owner of the given actor.
        std::function<void(actors::MoveActor&, characterevent::Event&)> ownerEvent;
        // 80067C30.
        std::function<void(actors::MoveActor&, const Vector&)> setVelocity;
        // Particle service for the effect owner's release (8008997C).
        std::function<void(const playermodels::VacuumPoint&, unsigned, float)> pointParticles;
        // 80089A0C: one motion step of the entry's beam model.
        std::function<void(playermodels::VacuumEffectOwner&)> effectMotion;
    };

    // One active entry: the original 0x168-byte object (fields not already in playermodels::VacuumEntry).
    struct Entry {
        std::int32_t mode = 0;                    // +0x04 (0 weak, 1 capture)
        bool started = false;                     // +0x08
        bool drain = false;                       // +0x09
        bool releasing = false;                   // +0x0A
        bool fieldB = false;                      // +0x0B
        bool caught = false;                      // +0x0C
        std::int32_t sourceCount = 0, targetCount = 0; // +0xB4 / +0xB8
        float strength = 0;                       // +0xBC
        std::uint32_t maximum = 0;                // +0xC0
        float fieldC4 = 0, fieldC8 = 0, fieldCC = 0, fieldD0 = 0, fieldD4 = 0, fieldD8 = 0, fieldDC = 0, fieldE0 = 0;
        std::uint32_t fieldE4 = 0;
        std::uint8_t fieldE8 = 0;
        Vector sourcePosition{}, sourceVelocity{}; // +0xEC / +0xF8
        Vector targetPosition{}, targetVelocity{}; // +0x104 / +0x110
        float distance = 0, horizontalDistance = 0; // +0x11C / +0x120
        Vector direction{}, horizontal{}, reverse{}, reverseHorizontal{}; // +0x124 / +0x130 / +0x13C / +0x148
        Vector beam{};                             // +0x154
        float previousTrackingScale = 0;           // +0x160
        bool previousTrackingFlag = false;         // +0x164
        bool outsideCone = false;                  // +0x165
        // Source record (0x68, 12 words) and target record (0x98, 7 words) as the original publishes them.
        std::int16_t sourceLife = 0, targetLife = 0;
        std::int32_t sourceWord1 = 0, sourceWord2 = 0;
        float sourceWord3 = 0, sourceWord5 = 0, sourceWord6 = 0, sourceWord7 = 0, sourceWord8 = 0;
        bool sourceModel = false;
        std::uint32_t flags = 0;
        std::uint32_t targetCountWord = 0, state = 0;
    };

    class Controller {
    public:
        Controller(playermodels::VacuumEntries& entries, Services services);
        // 80082854: add a source/target pair (mode 1 captures). Refuses duplicate or crossing pairs.
        bool add(actors::MoveActor& source, actors::MoveActor& target, int mode);
        // 80082E8C: does any active entry use this source?
        bool containsSource(const actors::MoveActor* actor) const;
        // 800829FC: per-frame entries update, then 80083108 / 80083534 / 800839C0.
        void update();
        const Entry& entry(unsigned index) const { return entries_.at(index); }
        // First actor of a target record's source list (the chain 80098ABC reads).
        const actors::MoveActor* firstSource(const std::array<std::uint32_t, 7>& targetRecord) const;
        std::array<std::array<actors::MoveActor*, 7>, 6> targetsOfSource{}, sourcesOfTarget{};
        // 80083B60: the aggregated source record of a source actor in the shape the player code reads (`field11A0`).
        playeractor::Target* captureTarget(const actors::MoveActor& source);
        std::array<Vector, 6> sourceVelocity{}, targetVelocity{};
        std::array<playeractor::Target, 6> sourceTargets{};

    private:
        playermodels::VacuumEntries& manager_;
        Services services_;
        std::array<Entry, 6> entries_;
        bool initialize(unsigned index, actors::MoveActor& source, actors::MoveActor& target, int mode);
        void updateEntry(unsigned index, float strength, std::int32_t sourceCount, std::int32_t targetCount,
            std::uint32_t maximum, bool flag);
        void geometry(unsigned index);
        void publish(unsigned index);
        void publishEffect(unsigned index);
        void fade(Entry& entry);
        void approach(unsigned index);
        void shape(Entry& entry);
        void capture(unsigned index);
        void far(unsigned index);
        void release(unsigned index);
        void idle(unsigned index);
        void pull(Entry& entry, double inner, double blend);
        void rebuildLists();
        void aggregate();
        void applyVelocities();
        playermodels::VacuumEntry& slot(unsigned index) { return *manager_.entries[index]; }
        int sourceIndex(const actors::MoveActor* actor) const;
        int targetIndex(const actors::MoveActor* actor) const;
        void stopQuiet(unsigned index);
        void stopWithPoints(unsigned index);
    };
}
