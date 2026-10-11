#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace playercomponents {
    struct EmptyState {
        EmptyState();
    };

    struct QuaternionState {
        explicit QuaternionState(const std::array<float, 4>& value);
        std::array<float, 4> quaternion;
        std::optional<float> field10;
    };

    struct State34 {
        State34();
        std::array<float, 3> field0;
        float fieldC, field10, field14;
        std::uint16_t field18, field1A;
        std::uint32_t field1C, field20;
        std::uint8_t field24;
        void* field28;
        std::uint8_t field2C, field2D;
        std::uint16_t field2E;
        std::uint32_t field30;
    };

    struct State28 {
        State28();
        void* field0;
        std::uint32_t field4;
        float field8;
        std::array<float, 3> fieldC, field18;
        std::uint32_t field24;
    };

    struct State80 {
        State80();
        std::array<float, 3> field0;
        std::uint32_t fieldC;
        std::uint8_t field10, field11;
        std::array<float, 12> field14;
        std::uint32_t field44;
        std::array<float, 12> field48;
        float field78;
        std::uint32_t field7C;
    };

    class BitBuffer {
    public:
        BitBuffer(std::uint8_t* data, unsigned count, unsigned bytes, unsigned width);
        virtual ~BitBuffer() = default;
        std::uint8_t* data;
        unsigned count, bytes, width;
    };

    class FlagBuffer : public BitBuffer {
    public:
        explicit FlagBuffer(std::uint8_t* data);
        std::uint32_t field14 = 0;
    };
}
