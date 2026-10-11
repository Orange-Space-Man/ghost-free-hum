#pragma once

#include <array>
#include <cstdint>

namespace playerevents {
    class Payload {
    public:
        explicit Payload(const std::array<std::uint8_t, 0x10c>& retained = {});
        void reset();
        std::uint32_t word(unsigned offset) const;
        void word(unsigned offset, std::uint32_t value);
        float number(unsigned offset) const;
        std::array<std::uint8_t, 0x10c> bytes;
    };
}
