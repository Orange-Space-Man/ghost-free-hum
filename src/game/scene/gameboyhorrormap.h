#pragma once

#include "game/world/doorresources.h"
#include "game/world/doordescriptors.h"
#include "game/world/roomlocation.h"

#include <array>
#include <cstdint>
#include <vector>

namespace gameboyhorror {
    struct MapRoom {
        std::uint8_t room = 0, floor = 0, area = 0;
        std::int16_t node = -1;
        std::uint8_t color = 0;
    };

    struct MapDoor {
        dooranimation::Matrix transform{};
        std::array<float, 3> position{};
        std::uint8_t floor = 0, savedId = 0, doorModel = 0, keyModel = 0;
        bool keyVisible = false;
    };

    class Map {
    public:
        bool load(const std::vector<std::uint8_t>& executable,
            const dooranimation::PoseTables& pose, const roomlocation::Rooms& rooms,
            const doordata::Table& doors, std::string& error);
        void refresh(const std::vector<std::uint32_t>& roomFlags,
            const std::uint8_t* savedFlags, std::size_t savedFlagBytes,
            const std::uint8_t* keyFlags, std::size_t keyFlagBytes);
        void open(std::uint32_t playerRoom, const std::array<float, 3>& playerPosition);
        void openKey(std::uint32_t playerRoom, const std::array<float, 3>& playerPosition, unsigned keyId);
        unsigned update(std::uint32_t held, std::uint32_t pressed, bool fullRotation = false);
        bool active() const;
        bool zoomedIn() const;
        int floor() const;
        int currentFloor() const;
        const std::array<float, 3>& target() const;
        float distance() const;
        std::int16_t yaw() const;
        std::int16_t pitch() const;
        float markerSize() const;
        float markerBounce() const;
        void camera(dooranimation::Matrix& view, std::array<float, 16>& projection) const;
        const doors::ModelResource& model() const;
        const doors::ModelResource& marker() const;
        const doors::ModelResource& doorModel(unsigned index) const;
        dooranimation::Matrix keyTransform(const MapDoor& marker) const;
        const std::vector<MapDoor>& doorMarkers() const;
        const std::vector<MapRoom>& rooms() const;
        const std::array<std::array<std::uint8_t, 4>, 7>& colors() const;

    private:
        doors::ModelResource mapModel;
        doors::ModelResource playerMarker;
        dooranimation::PoseTables poseTables;
        std::array<doors::ModelResource, 7> doorModels;
        std::vector<MapDoor> mapDoors;
        std::vector<MapRoom> mapRooms;
        std::array<std::array<std::uint8_t, 4>, 7> mapColors{};
        std::array<float, 5> floorHeights{};
        std::array<float, 3> cameraTarget{0.0f, 10.0f, -257.5f};
        std::array<float, 3> overviewTarget{0.0f, 10.0f, -257.5f};
        std::array<float, 3> floorTransitionTarget{0.0f, 10.0f, -257.5f};
        std::array<float, 3> markerPosition{};
        float cameraDistance = 1600.0f;
        std::int16_t cameraYaw = 0x1200;
        std::int16_t cameraPitch = 0x1900;
        int selectedFloor = 0, playerFloor = 0, floorStep = 0;
        unsigned floorFrame = 0, idleFrames = 0;
        unsigned keyFocusFrame = 0;
        std::uint16_t markerAngle = 0;
        std::uint16_t keyAngle = 0;
        bool opened = false, zoomed = false;
        bool keyFocusTransition = false;
    };
}
