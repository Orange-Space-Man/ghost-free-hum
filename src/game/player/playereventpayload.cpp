#include "game/player/playereventpayload.h"

#include <cstring>
#include <stdexcept>

playerevents::Payload::Payload(const std::array<std::uint8_t, 0x10c>& retained) : bytes(retained) { reset(); }

std::uint32_t playerevents::Payload::word(unsigned offset) const {
    if (offset > bytes.size() - 4) throw std::out_of_range("Player event payload word.");
    return (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
        (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
}

void playerevents::Payload::word(unsigned offset, std::uint32_t value) {
    if (offset > bytes.size() - 4) throw std::out_of_range("Player event payload word.");
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
}

float playerevents::Payload::number(unsigned offset) const {
    const auto bits = word(offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void playerevents::Payload::reset() {
    for (unsigned offset : {0x04u, 0x0cu, 0x10u, 0x24u, 0x2cu, 0x34u, 0x3cu, 0x40u, 0x44u, 0x50u,
        0x5cu, 0x60u, 0x64u, 0x68u, 0x70u, 0x78u, 0x7cu, 0x80u, 0x84u, 0x8cu, 0x90u, 0x94u,
        0x98u, 0xa4u, 0xacu, 0xc0u, 0xd4u, 0xd8u, 0xdcu, 0xecu, 0xf0u, 0xf4u, 0xf8u, 0x104u}) word(offset, 0);
    for (unsigned offset : {0x08u, 0x14u, 0x30u, 0x38u, 0x6cu, 0xb8u, 0xc8u, 0xccu, 0xd0u, 0xe0u, 0x100u, 0x108u})
        word(offset, 0xffffffff);
    for (unsigned offset : {0x00u, 0x18u, 0x74u, 0x9cu, 0xb0u}) word(offset, 10);
    word(0x20, 3);
    word(0x28, 5);
    word(0xa0, 5);
    word(0x48, 100);
    word(0x4c, 100);
    word(0x54, 0x3f800000);
    word(0x58, 1);
    word(0xb4, 0x8021da40);
    word(0xbc, 0x8021da40);
    word(0xe4, 0xbf800000);
    word(0xe8, 0x3f800000);
    word(0xfc, 0x3e99999a);
    bytes[0x1c] = bytes[0xa8] = bytes[0xc4] = 0;
    bytes[0xc5] = 1;
}
