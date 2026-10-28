#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace actors { class MoveActor; }

namespace playercomponents {
    struct EmptyState {
        EmptyState();
    };

    struct QuaternionState {
        explicit QuaternionState(const std::array<float, 4>& value);
        std::array<float, 4> quaternion;
        std::optional<float> field10;
    };

    struct DoorState {
        std::array<float, 3> approachPosition{}, facing{0, 0, 1};
        std::int16_t door = 0, pairedDoor = -1;
        std::uint32_t sourceRoom, side = 1;
        std::uint8_t command = 1;
        std::uint32_t approachFrames = 0;
        std::uint8_t unlocked = 0, blocked = 0;
        std::uint16_t flags = 1;
        actors::MoveActor* fakeDoor;
    };

    struct LadderState {
        std::uint32_t ladder = 0, step = 0;
        float distance = 0;
        std::array<float, 3> approachPosition{}, facing{0, 0, 1};
        std::uint32_t entrySide = 0;
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
