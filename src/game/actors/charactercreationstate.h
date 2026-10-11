#pragma once

#include "game/world/roomlightpool.h"

#include "game/actors/characterowner.h"
#include "game/player/playerrecordwriteback.h"
#include "game/actors/moveactor.h"

namespace charactercreation {
    class Properties {
    public:
        Properties(const std::vector<std::uint8_t>& executable, const enemycreation::Catalog& catalog,
            const enemytypes::Manager& parameters);
        int savedIndex(unsigned type) const;
        int group(unsigned type) const;
        std::uint32_t life(unsigned type, const playerrecord::Record& player) const;
        void initializeLife(actors::MoveActor& actor, characterowner::State& state, const playerrecord::Record& player) const;
        std::uint32_t color(unsigned type) const;
        void writeColor(actors::MoveActor& actor, std::uint32_t value) const;
        void updateEffects(const actors::MoveActor& actor, roomlighting::Pool& lights) const;

    private:
        const enemycreation::Catalog& catalog;
        const enemytypes::Manager& parameters;
        std::array<unsigned, 23> savedTypes;
    };
}
