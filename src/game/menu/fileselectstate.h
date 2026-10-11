#pragma once

#include <array>
#include <vector>

namespace fileselect {
    enum class Action { copy, erase, format, selectFile, applyOptions, acknowledgeDamage, openRumble, closeRumble };
    struct Request {
        Action action;
        unsigned source = 0;
        unsigned destination = 0;
    };

    struct Input {
        unsigned pressed = 0;
        unsigned repeat = 0;
    };

    struct State {
        unsigned mode = 0;
        unsigned selection = 0;
        unsigned confirmation = 0;
        unsigned copyDestination = 0;
        unsigned optionSelection = 0;
        std::array<unsigned, 3> options{1, 1, 0};
        unsigned frames = 0;
        unsigned timer = 0;
        unsigned cursorPhase = 0;
        bool rumbleOpen = false;
        int requestedScene = -1;
        int selectedFile = -1;
        std::vector<unsigned> sounds;
        std::vector<Request> requests;

        void update(Input input, const std::array<bool, 3>& used, unsigned cardStatus, bool damaged, bool ready);
    };
}
