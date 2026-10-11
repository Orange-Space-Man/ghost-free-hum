#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace doordata {
    struct Descriptor {
        std::array<std::uint8_t, 4> attributes{};
        std::uint32_t identity = 0;
        std::array<std::int32_t, 3> center{};
        std::array<std::int16_t, 3> dimensions{};
        std::array<std::uint8_t, 2> rooms{};
    };

    struct Table {
        std::vector<Descriptor> entries;
        std::uint16_t liveCount = 0;
    };

    bool read(const std::vector<std::uint8_t>& executable, std::uint32_t address, Table& table, std::string& error);
    bool traversalEnabled(const Descriptor& descriptor);
}
