#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace roomtraversal {
    using Bounds = std::array<std::int16_t, 4>;

    struct Record {
        std::uint16_t flags = 0;
        std::uint8_t depth = 0;
        std::int8_t marker = 0;
        Bounds bounds{};
    };

    class State {
    public:
        void initialize(unsigned roomCount);
        void reset(unsigned buffer, std::int16_t roomCount);
        void mark(unsigned buffer, unsigned map, std::uint32_t roomId,
            std::uint32_t& roomFlags, std::uint16_t& savedRoomFlags,
            std::uint8_t depth, std::int8_t marker, std::uint8_t clippingMode,
            const Bounds& clippedBounds, const Bounds& fullBounds);
        bool marked(unsigned roomIndex, int buffer, unsigned currentBuffer) const;
        const Record& record(std::uint32_t roomId, unsigned buffer) const;
        std::int8_t rawMarker(unsigned roomIndex, unsigned buffer) const;
        std::int8_t visibleMarker(std::uint32_t roomId, unsigned buffer) const;
        unsigned visitCount(unsigned buffer) const;
        std::uint8_t visit(unsigned buffer, unsigned index) const;

    private:
        struct Buffer {
            std::vector<Record> rooms;
            std::array<std::uint8_t, 64> visits{};
            unsigned count = 0;
        };
        std::array<Buffer, 8> buffers;
    };
}
