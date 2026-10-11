#pragma once

#include "game/player/playervacuummodel.h"

namespace actors { class MoveActor; }

namespace playermodels {
    struct VacuumEffectGlobals {
        std::array<VacuumParameters, 2> parameters;
        void initializeOwner();
    };

    class VacuumEffectOwner {
    public:
        VacuumEffectOwner(const SharedModels& shared, VacuumEffectGlobals& globals);
        std::uint8_t state = 0;
        std::unique_ptr<VacuumModel> model;
        std::array<std::uint8_t, 0x64> fields8;
    };

    struct VacuumTracking {
        void reset();
        VacuumTracking();
        std::array<float, 3> field0, fieldC, field18;
        float field24, field28, field2C;
        std::uint8_t field30, field31;
        std::uint32_t field34;
        std::uint8_t field38, field39;
    };

    struct VacuumTrackingBlend {
        float field0 = 0;
        std::uint32_t field4 = 0;
        std::uint8_t field8 = 0;
        void reset();
    };

    struct VacuumEntry {
        VacuumEntry(const SharedModels& shared, VacuumEffectGlobals& globals);
        VacuumTracking tracking;
        VacuumTrackingBlend blend;
        std::uint8_t state = 0, field8 = 0;
        actors::MoveActor* source = nullptr;
        actors::MoveActor* target = nullptr;
        std::uint8_t trackingChanged = 0, blendChanged = 0;
        std::unique_ptr<VacuumEffectOwner> effect;
        void setTracking(const VacuumTracking& value);
        void setBlend(const VacuumTrackingBlend& value);
    };

    class VacuumEntries {
    public:
        VacuumEntries(const SharedModels& shared, VacuumEffectGlobals& globals);
        ~VacuumEntries();
        void reset();
        bool containsSource(const actors::MoveActor* actor) const;
        bool containsTarget(const actors::MoveActor* actor) const;
        VacuumEntry* findSource(const actors::MoveActor* actor, int ordinal) const;
        VacuumEntry* findTarget(const actors::MoveActor* actor, int ordinal) const;
        bool setTracking(const actors::MoveActor* actor, const VacuumTracking& value);
        bool setBlend(const actors::MoveActor* actor, const VacuumTrackingBlend& value);
        int sourceIndex(const actors::MoveActor* actor) const;
        int targetIndex(const actors::MoveActor* actor) const;
        std::array<std::uint32_t, 12>* sourceRecord(const actors::MoveActor* actor);
        std::array<std::uint32_t, 7>* targetRecord(const actors::MoveActor* actor);
        std::array<std::unique_ptr<VacuumEntry>, 6> entries;
        std::array<std::array<std::uint32_t, 12>, 6> sourceRecords;
        std::array<std::array<std::uint32_t, 7>, 6> targetRecords;
        std::array<actors::MoveActor*, 6> fieldA38, fieldA50;
        std::array<std::uint32_t, 6> fieldBB8, fieldBD0;
        std::array<float, 6> fieldBE8;
        std::uint32_t fieldC90, fieldC94, fieldC98 = 0;
    };
}
