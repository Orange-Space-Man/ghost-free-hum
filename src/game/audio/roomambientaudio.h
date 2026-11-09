#pragma once

#include "game/audio/gameplayaudiocontext.h"
#include "game/audio/jaudiopositionalplayback.h"

namespace gameplayaudio {
    class RoomAmbience {
    public:
        RoomAmbience(const std::vector<std::uint8_t>& executable, jaudio::PositionalPlayback& playback);
        void update(const RoomParameters& parameters, const std::array<float, 12>& listener);

    private:
        jaudio::PositionalPlayback& playback;
        std::array<float, 3> windPosition;
    };
}
