#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace roomlocation {
    using Bounds = std::array<std::int32_t, 6>;

    struct Room {
        std::uint32_t id;
        Bounds bounds;
        std::vector<Bounds> regions;
        std::array<std::uint8_t, 4> ambient{};
    };

    class Rooms {
    public:
        explicit Rooms(std::vector<Room> records);
        Rooms(const std::vector<std::uint8_t>& executable, unsigned map);
        unsigned resolve(const std::array<float, 3>& position, unsigned previous = 0xffffffff) const;
        bool contains(unsigned index, const std::array<float, 3>& position) const;
        const std::vector<Room>& entries() const;

    private:
        std::vector<Room> records;
    };
}
