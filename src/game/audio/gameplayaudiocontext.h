#pragma once

#include "game/events/missionresources.h"
#include "game/events/runtimeflags.h"

#include <array>

namespace gameplayaudio {
    struct RoomParameters {
        std::uint32_t packed = 0;
        unsigned size() const;
        unsigned musicCode() const;
        unsigned material() const;
    };

    class Context {
    public:
        Context(const mission::Resources& resources, const gameflags::State& flags, unsigned map);
        RoomParameters parameters(unsigned room, std::int16_t soundPolygon) const;

    private:
        const jmp::ToolData* roomInfo;
        const jmp::ToolData* soundInfo;
        const gameflags::State& flags;
        unsigned map;
        bool floorFields = false;
        std::array<std::uint16_t, 256> roomRows;
    };
}
