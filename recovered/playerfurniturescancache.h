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

    struct FurnitureContact {
        Vector center;
        float minimumY, maximumY;
        unsigned sound;
    };

    struct FurnitureInput {
        Vector position, direction;
        float radius, height, reachHeight, roomCenterY;
        bool inverted = false;
    };

    struct FurnitureInteraction {
        int target = -1;
        Vector direction{};
        unsigned reaction = 0, sound = 0;
        bool invert = false;
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
        const std::vector<FurnitureContact>& contacts() const;
        FurnitureInteraction press(const FurnitureInput& input, const std::array<std::uint8_t, 4>& activeSheets,
            const std::function<std::uint32_t()>& random) const;
        FurnitureInteraction automatic(const FurnitureInput& input) const;

    private:
        roomfurniture::Owner& owner;
        const gameflags::State& flags;
        actormodel::InitialPose poses;
        unsigned room = 0xffffffff;
        float maximum, half, angleScale;
        std::vector<std::uint16_t> rows;
        std::vector<FurnitureTarget> entries;
        std::vector<FurnitureContact> contactBounds;
        float contactMargin, randomScale, inversionMargin, frontThreshold;
        std::array<int, 4> sheets{-1, -1, -1, -1};
    };
}
