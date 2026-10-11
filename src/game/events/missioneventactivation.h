#pragma once

#include "game/events/eventinfodecoder.h"
#include "game/scene/gameentrystate.h"
#include "game/events/missionresources.h"

namespace gameevent {
    class MissionActivation {
    public:
        MissionActivation(const mission::Resources& resources, const enemytypes::Catalog& types,
            const gameentry::State& entry, const gameflags::State& flags);
        void updateAppearance(std::int32_t elapsedSeconds);
        const ActivationState& activation() const;
        const std::vector<ConditionState>& conditions() const;

    private:
        const gameflags::State& flags;
        ActivationState active;
        std::vector<ConditionState> records;
    };
}
