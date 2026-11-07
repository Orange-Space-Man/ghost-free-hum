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
        const Definition* definition(unsigned number) const;
        ConditionState& condition(unsigned number);
        void complete(unsigned number, std::uint8_t& completedCount);

    private:
        const gameflags::State& flags;
        ActivationState active;
        std::vector<ConditionState> records;
        std::vector<Definition> definitions;
    };
}
