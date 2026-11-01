#pragma once

#include "game/player/playeractioncontext.h"
#include "game/audio/jaudiopositionalplayback.h"

namespace playeractor {
    struct AudioFrameServices {
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        const mapcollision::State& collision;
        jaudio::PositionalPlayback& playback;
        std::uint32_t audioFrame;
        std::function<void(unsigned, std::int16_t)> roomContext;
        std::function<bool(unsigned)> sequenceActive;
        std::function<std::int32_t(unsigned)> feedbackRepeats;
    };

    class AudioFrame {
    public:
        explicit AudioFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, const AudioFrameServices& services);
        std::array<std::uint32_t, 2> footsteps{};
        float movementSpeed = 0;

    private:
        playeractions::Context actions;
        std::array<float, 11> movement;
        std::array<float, 2> scraping;
        float landingPulse, slipPulse;
    };
}
