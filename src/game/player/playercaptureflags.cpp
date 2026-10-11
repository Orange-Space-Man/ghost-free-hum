#include "game/player/playercaptureflags.h"

#include <stdexcept>

namespace {
    std::uint8_t& byte(playercomponents::BitBuffer& buffer, unsigned bit) {
        if (!buffer.data || (bit >> 3) >= buffer.bytes) {
            throw std::runtime_error("Original player event bit index exceeds its bound storage.");
        }
        return buffer.data[bit >> 3];
    }

    bool insert(playercomponents::BitBuffer& buffer, unsigned bit) {
        if (!buffer.data) return false;
        auto& value = byte(buffer, bit);
        const auto mask = static_cast<std::uint8_t>(1u << (bit & 7));
        if (value & mask) return false;
        value |= mask;
        return true;
    }
}

bool playerevents::registerCapture(playercomponents::BitBuffer& captured, playercomponents::BitBuffer& grades,
    const charactercreation::Properties& properties, unsigned type, std::uint16_t life) {
    if (!insert(captured, type)) return false;
    if (type == 0x40 || type == 0xd5 || type == 0xdc || type == 0xdd) {
        const auto signedLife = static_cast<std::int16_t>(life);
        unsigned grade = signedLife < 60 ? 1 : signedLife < 90 ? 2 : 3;
        const int index = properties.savedIndex(type);
        if (index < 0) throw std::runtime_error("Original capture grade has no saved actor index.");
        const unsigned first = unsigned(index) * grades.width;
        unsigned bit = (unsigned(index) + 1) * grades.width;
        while (bit != first) {
            --bit;
            auto& value = byte(grades, bit);
            const auto mask = static_cast<std::uint8_t>(1u << (bit & 7));
            value = grade & 1 ? value | mask : value & static_cast<std::uint8_t>(~mask);
            grade >>= 1;
        }
    }
    return true;
}

bool playerevents::registerSeen(playercomponents::BitBuffer& seen, unsigned type) {
    return insert(seen, type);
}

void playerevents::promoteGrade(playercomponents::BitBuffer& grades, std::int32_t index, std::int32_t grade) {
    if (index < 0 || grade < 0) return;
    const unsigned first = unsigned(index) * grades.width;
    const unsigned end = (unsigned(index) + 1) * grades.width;
    unsigned current = 0;
    for (unsigned bit = first; bit < end; ++bit) {
        current = (current << 1) | ((byte(grades, bit) >> (bit & 7)) & 1);
    }
    if (current >= unsigned(grade)) return;
    unsigned value = unsigned(grade);
    for (unsigned bit = end; bit != first;) {
        --bit;
        auto& destination = byte(grades, bit);
        const auto mask = static_cast<std::uint8_t>(1u << (bit & 7));
        destination = value & 1 ? destination | mask : destination & static_cast<std::uint8_t>(~mask);
        value >>= 1;
    }
}
