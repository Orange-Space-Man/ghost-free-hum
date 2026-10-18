#pragma once

#include <array>
#include <cstdint>

namespace jaudio {
    class SequenceRandom {
    public:
        std::uint32_t next(std::uint32_t tick);

    private:
        std::array<std::uint8_t, 8> state{0x12, 0x34, 0x56, 0x67, 0xa5, 0xb4, 0xc3, 0xb1};
    };

    std::uint32_t sequenceRandom();
}
