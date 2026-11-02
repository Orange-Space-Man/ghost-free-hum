#pragma once

#include "game/world/roomfurnitureowner.h"
#include "game/enemies/itemcreationroute.h"

#include <functional>

namespace roomfurniture {
    using Vector = std::array<float, 3>;

    struct ResponseServices {
        std::function<bool(unsigned)> roomCleared;
        std::function<jmp::ToolDataRef(unsigned)> selectItem;
        std::function<unsigned(const jmp::ToolDataRef&, const Vector&)> createItem;
        std::function<void(unsigned, const Vector&)> createType;
        std::function<void(bool, float, const Vector&, const Vector&, unsigned)> scatter;
        std::function<void(unsigned, const Vector&)> effect;
        std::function<void(unsigned, const Vector&, const float*)> sound;
        std::function<void(unsigned, const Vector&)> releaseOccupant;
        std::function<void()> persistCollected;
        std::function<void(Object&, unsigned)> animation;
        std::function<float(float, const Vector&)> suctionStrength;
        std::function<void(unsigned)> scanMessage;
    };

    class Response {
    public:
        Response(const std::vector<std::uint8_t>& executable, const characteritems::Admission& admission);
        unsigned strike(Object& object, RowState& row, bool animate, bool suppressCollection,
            const ResponseServices& services) const;
        void suction(Object& object, int strength, bool animate, unsigned frame, const ResponseServices& services) const;
        void collect(Object& object, RowState& row, bool forced, const ResponseServices& services,
            const Vector* offset = nullptr, const float* retainedLaunchSpeed = nullptr) const;
        void occupant(Object& object, RowState& row, bool forced, const ResponseServices& services) const;
        void basic(Object& object, RowState& row, bool visible, bool vacuumActive, unsigned animationKind,
            unsigned frame, const ResponseServices& services) const;

    private:
        const characteritems::Admission& admission;
        dooranimation::PoseTables trig;
        std::array<unsigned, 26> sounds{};
        std::array<float, 3> ceilingDirection{};
        float ceilingOffset = 0, floorOffset = 0, directionScale = 0, floorSpeed = 0, ceilingSpeed = 0;
        float suctionRadius = 0;
        Vector position(const Object& object, const RowState& row) const;
        Vector direction(const Object& object) const;
        void dust(const Object& object, const Vector& position, const ResponseServices& services) const;
        void appeared(const Vector& position, const ResponseServices& services) const;
    };
}
