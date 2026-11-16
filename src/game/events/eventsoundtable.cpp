#include "game/events/eventsoundtable.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <array>
#include <stdexcept>

unsigned gameevent::soundId(const std::vector<std::uint8_t>& executable, unsigned index) {
    constexpr std::array<unsigned, 55> supported{0, 1, 2, 3, 4, 5, 6, 7, 8, 101, 102, 111, 150, 153, 176, 191, 218, 212, 136, 195, 196, 219, 202, 200, 137, 141, 189, 215, 45, 77, 109, 117, 118, 129, 142, 143, 144, 151, 158, 163, 164, 165, 166, 167, 168, 181, 182, 183, 184, 190, 192, 193, 204, 216, 220};
    if (std::find(supported.begin(), supported.end(), index) == supported.end())
        throw std::runtime_error("Event sound index is outside the recovered event scope.");
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x8038e728 + index * 4, 4, bytes, error)) throw std::runtime_error(error);
    return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
}
