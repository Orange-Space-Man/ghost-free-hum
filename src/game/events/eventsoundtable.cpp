#include "game/events/eventsoundtable.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <array>
#include <stdexcept>

unsigned gameevent::soundId(const std::vector<std::uint8_t>& executable, unsigned index) {
    constexpr std::array<unsigned, 17> supported{0, 1, 2, 3, 4, 5, 6, 7, 8, 101, 102, 111, 150, 153, 176, 191, 218};
    if (std::find(supported.begin(), supported.end(), index) == supported.end())
        throw std::runtime_error("Event sound index is outside the recovered event scope.");
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x8038e728 + index * 4, 4, bytes, error)) throw std::runtime_error(error);
    return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
}
