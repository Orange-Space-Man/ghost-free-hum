#include "game/audio/roomambientaudio.h"
#include "resources/dolresources.h"

#include <cstring>
#include <stdexcept>

gameplayaudio::RoomAmbience::RoomAmbience(const std::vector<std::uint8_t>& executable, jaudio::PositionalPlayback& playback) :
    playback(playback) {
    constexpr unsigned windPositionAddress = 0x8038e710;
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, windPositionAddress, 12, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < windPosition.size(); ++i) {
        const unsigned bits = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
            (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
        std::memcpy(&windPosition[i], &bits, 4);
    }
}

void gameplayaudio::RoomAmbience::update(const RoomParameters& parameters, const std::array<float, 12>& listener) {
    if (parameters.musicCode() != 5) return;
    windPosition[0] = -(listener[0] * listener[3] + listener[4] * listener[7] + listener[8] * listener[11]);
    playback.play(0x103c, windPosition);
}
