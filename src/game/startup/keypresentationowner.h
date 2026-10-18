#pragma once

#include "game/startup/keypresentationstate.h"
#include "game/startup/openingscene.h"
#include "game/events/runtimeflags.h"
#include "game/audio/gameplaymusicowner.h"

#include <memory>

namespace keypresentation {
    struct SceneControl {
        unsigned state = 0, doorTransition = 0;
        bool gameplayEnabled = true;
    };

    class Owner {
    public:
        Owner(const std::filesystem::path& package, const std::vector<std::uint8_t>& executable, SceneControl& control, gameflags::State& flags,
            gameplaymusic::Owner& music);
        ~Owner();
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        bool request(bool missingKey, bool front, unsigned doorType);
        void advanceDraw(unsigned pressed);
        void draw(bool present);
        void release();
        const State& state() const;

    private:
        Services services();
        std::filesystem::path package;
        SceneControl& control;
        gameflags::State& flags;
        gameplaymusic::Owner& music;
        State retained;
        std::array<unsigned, 56> modes{};
        std::unique_ptr<openingscene::Scene> movie;
        bool flag56 = false;
    };
}
