#pragma once

#include <array>
#include <cstdint>

namespace playerrecord {
    using Record = std::array<std::uint8_t, 0x16c>;

    struct Values {
        std::uint16_t life, maximumLife, flags;
        std::array<std::uint32_t, 64> entries;
        std::uint32_t count, field12F4, field12F8;
        std::array<std::uint32_t, 7> fields12FC;
        std::array<std::uint32_t, 3> fields1318;
        std::uint32_t controlWord;
    };

    bool write(Record* destination, const Values& values);
}
