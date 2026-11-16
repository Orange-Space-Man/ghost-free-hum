#pragma once

#include "game/menu/controllerinstructionowner.h"
#include "game/player/playereventdispatch.h"
#include "game/save/gamesavestate.h"
#include <functional>

namespace controllerinstructions {
    class GameplayServices final : public Services {
    public:
        GameplayServices(gamesave::State& saves, playerevents::Dispatcher& players,
            std::function<void(unsigned)> playSound);
        unsigned option() const override;
        void setOption(unsigned value) override;
        void playerScheme(unsigned scheme) override;
        void sound(unsigned id) override;
        void saveOptions() override;

    private:
        gamesave::State& saves;
        playerevents::Dispatcher& players;
        std::function<void(unsigned)> playSound;
        std::array<std::uint8_t, 16> options;
    };
}
