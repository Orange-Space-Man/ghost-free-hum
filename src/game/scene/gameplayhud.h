#pragma once

#include "game/scene/gamearchive.h"
#include "game/startup/startupscene.h"
#include "game/player/playeractorview.h"
#include <map>

namespace gameplay {
    class Hud {
    public:
        Hud(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable, const std::vector<unsigned>& rooms);
        void update(const playeractor::View& player, bool fullscreen, bool roomCleared);
        std::vector<startupscene::Picture> pictures() const;
        const std::vector<roommaterials::Texture>& textures() const;

    private:
        std::vector<roommaterials::Texture> images;
        std::map<unsigned, std::size_t> roomTextures;
        std::array<std::size_t, 10> digits{};
        std::array<std::size_t, 3> hearts{};
        std::array<std::size_t, 5> floors{};
        std::vector<float> sine;
        unsigned previousRoom = 0xffffffff, selectedRoom = 0xffffffff, pendingRoom = 0xffffffff;
        unsigned roomPhase = 0, roomCounter = 0, roomDelay = 0;
        unsigned floorPhase = 0, floorCounter = 0, selectedFloor = 0xffffffff;
        unsigned pulseAngle = 0, pulsePeriod = 60;
        int life = 100, maximumLife = 100, slide = 0;
        unsigned reaction = 0;
        bool cleared = false;
        float digitScale = 0;
        unsigned healthAlpha = 0, slideStep = 0;
        std::size_t load(const gamearchive::Archive& archive, const std::string& path);
        startupscene::Picture picture(std::size_t texture, int x, int y, float scale,
            bool centered, std::array<float, 4> color) const;
    };
}
