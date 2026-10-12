#include "game/player/playercomponentstate.h"

playercomponents::EmptyState::EmptyState() {}

playercomponents::QuaternionState::QuaternionState(const std::array<float, 4>& value) : quaternion(value) {}

playercomponents::State28::State28() : field0(nullptr), field4(0), field8(0), fieldC{}, field18{0, 0, 1}, field24(0) {}

playercomponents::State80::State80() : field0{0, 0, 1}, fieldC(0), field10(0), field11(0),
    field14{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, field44(0),
    field48{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, field78(0), field7C(0) {}

playercomponents::BitBuffer::BitBuffer(std::uint8_t* data, unsigned count, unsigned bytes, unsigned width) :
    data(data), count(count), bytes(bytes), width(width) {}

playercomponents::FlagBuffer::FlagBuffer(std::uint8_t* data) : BitBuffer(data, 256, 32, 1) {}
