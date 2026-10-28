#pragma once

#include "playerscannerinteraction.h"
#include "game/world/roomfurnitureowner.h"
#include "game/actors/actorinitialpose.h"

#include <functional>

namespace playerscanner {
    struct SheetInput {
        unsigned slot, target, row, kind, texture;
        actormodel::Matrix matrix;
        Vector extent;
        bool occupied, elastic;
    };

    class FurnitureCache {
    public:
        FurnitureCache(const std::vector<std::uint8_t>& executable, roomfurniture::Owner& owner, const gameflags::State& flags);
        bool refresh(unsigned room, const std::function<void(const SheetInput&)>& initializeSheet);
        Hit query(const Vector& position, const Vector& direction) const;
        void mark(unsigned target);
        bool mirror(unsigned target) const;
        bool occupied(unsigned target) const;
        unsigned row(unsigned target) const;
        const std::vector<FurnitureTarget>& targets() const;

    private:
        roomfurniture::Owner& owner;
        const gameflags::State& flags;
        actormodel::InitialPose poses;
        unsigned room = 0xffffffff;
        float maximum, half, angleScale;
        std::vector<std::uint16_t> rows;
        std::vector<FurnitureTarget> entries;
        std::array<int, 4> sheets{-1, -1, -1, -1};
    };
}
