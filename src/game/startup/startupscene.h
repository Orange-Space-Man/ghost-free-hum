#pragma once

#include "game/startup/startupresources.h"
#include <array>

namespace startupscene {
    struct Picture {
        std::size_t texture = 0;
        float x = 0;
        float y = 0;
        float width = 0;
        float height = 0;
        float alpha = 1;
        std::array<float, 4> black{0, 0, 0, 0};
        std::array<float, 4> white{1, 1, 1, 1};
        std::array<std::array<float, 2>, 4> coordinates{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
        std::array<std::array<float, 4>, 4> colors{{{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}}};
        std::array<float, 6> transform{1, 0, 0, 1, 0, 0};
    };

    class Scene {
    public:
        void reset();
        void enterTitle();
        void update(bool audioFinished, bool startPressed, bool titleInitializationFinished = true);
        std::vector<Picture> pictures(const startupresources::Resources& resources) const;
        bool isBoot() const;
        int requestedScene() const;
        unsigned takeSoundRequest();
        unsigned titleState() const;

    private:
        unsigned bootState = 0;
        bool boot = true;
        unsigned titlePhase = 0;
        unsigned state = 0;
        int fade = 0;
        int wait = 60;
        int timeout = 1800;
        bool startLatched = false;
        int request = -1;
        unsigned soundRequest = 0;
        unsigned flash1 = 0;
        unsigned flash2 = 0;
        bool ready = false;
        unsigned backgroundAlpha = 0;
        unsigned logoAlpha = 0;
        unsigned copyrightAlpha = 0;
        unsigned startAlpha = 255;
        bool logoVisible = false;
        bool copyrightVisible = false;
        bool startVisible = false;
        void updateTitle(bool startPressed);
    };
}
