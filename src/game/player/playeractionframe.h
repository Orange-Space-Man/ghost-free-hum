#pragma once

#include "game/player/playeractioncontext.h"

namespace playeractions {
    struct FrameServices {
        gameflags::State& flags;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        std::function<void(std::uint32_t)> globalSound;
        std::function<void(std::uint32_t, const std::array<float, 3>&)> positionalSound;
    };

    class Frame {
    public:
        Frame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose);
        void run(playeractor::Player& player, const mapcollision::State& collision, const FrameServices& services) const;
        void apply(playeractor::Player& player, unsigned animation, unsigned previousMode, const FrameServices& services) const;
        unsigned fallbackDuration(unsigned entry, const actormodel::Resources& models) const;

    private:
        Context context;
        Update update;
        Actions fallback;
    };
}
