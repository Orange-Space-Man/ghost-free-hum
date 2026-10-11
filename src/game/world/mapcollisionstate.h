#pragma once

#include "game/events/missionresources.h"
#include <array>

namespace mapcollision {
    struct SphereContact {
        std::array<float, 3> correction{}, floorNormal{}, wallNormal{};
        int floor = -1, wall = -1;
    };

    class State {
    public:
        explicit State(const mission::Resources& resources);
        void load(const std::vector<std::uint8_t>& collision);
        void enable(unsigned bit);
        void disable(unsigned bit);
        std::uint16_t mask() const;
        int soundMaterialField() const;
        int soundEchoField() const;
        const jmp::ToolData* polygonInfo() const;
        float floorHeight(const std::array<float, 3>& position) const;
        std::array<std::uint16_t, 4> floorPolygons(const std::array<float, 3>& position, float distance) const;
        bool projectFloor(std::array<float, 3>& position) const;
        bool polygonNormal(int polygon, std::array<float, 3>& normal) const;
        bool ladderContact(std::uint16_t polygon, const std::array<float, 3>& position) const;
        SphereContact sphereContact(const std::array<float, 3>& position, float radius) const;
        SphereContact sweepSphere(const std::array<float, 3>& position, float radius,
            const std::array<float, 3>& displacement) const;

    private:
        const mission::Resources& resources;
        std::vector<std::uint8_t> collision;
        std::uint16_t enabled = 0x8000;
        int soundMaterial = -1, soundEcho = -1;
    };
}
