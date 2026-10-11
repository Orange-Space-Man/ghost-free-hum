#pragma once

#include "game/world/roomfurnitureresources.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"

namespace roomfurniture {
    struct RowState {
        std::uint32_t flags = 0;
        std::int16_t occupant = -1;
        std::uint8_t height = 0;
    };

    struct Object {
        std::uint32_t flags = 0, room = 0, behavior = 0, updates = 0;
        std::uint16_t row = 0;
        std::uint8_t animation = 0, opacity = 255;
        std::array<float, 3> position{}, scale{1, 1, 1};
        std::array<std::int32_t, 3> angles{}, arguments{};
        std::uint32_t moveLevel = 0, generate = 0, sound = 0, itemTable = 0, generateCount = 0, counter = 0;
        std::int32_t itemOffset = 0;
        float frame = 0, previousFrame = 0, frameDelta = 0;
        std::array<float, 2> swing{}, swingVelocity{};
        const doors::ModelResource* model = nullptr;
        std::vector<const Animation*> animations;
        dooranimation::Matrix world{};
        std::vector<dooranimation::Matrix> matrices;
    };

    class Owner {
    public:
        Owner(const std::vector<std::uint8_t>& executable, const roomlocation::Rooms& rooms,
            const jmp::ToolData& table, const std::array<std::uint8_t, 0x220>& inventory, unsigned map);
        void create(const Resources& resources, const jmp::ToolData& table);
        void initializeObjects();
        void updateIdle(const roomtraversal::State& visibility, unsigned buffer);
        void updateOpacity(float playerZ);
        void release(unsigned row);
        void releaseAll();
        const Object* object(unsigned row) const;
        const std::array<Object, 80>& slots() const;
        const RowState& rowState(unsigned row) const;

    private:
        const roomlocation::Rooms& rooms;
        dooranimation::PoseTables poseTables;
        double angleBias = 0, angleScale = 0;
        float previousFrame = 0;
        std::array<float, 4> spring{};
        float quantization = 0, inverseQuantization = 0;
        float opacityDistance = 0;
        std::uint8_t minimumOpacity = 0;
        std::array<Object, 80> objects;
        std::vector<int> rows;
        std::vector<RowState> states;
        Object construct(const Placement& placement, const Resources& resources, const jmp::ToolData& table) const;
        void refreshPose(Object& object);
    };
}
