#pragma once

#include "game/startup/openingresources.h"

namespace openingscene {
    struct Command {
        unsigned opcode = 0, model = 0, animation = 0;
        float frame = 0;
    };

    class Timeline {
    public:
        void reset(const Resources& resources);
        void update(const Resources& resources, bool flag55);
        void afterDraw(const Resources& resources);
        unsigned segment = 0, flags = 0, event = 0;
        float frame = 0, drawFrame = 0, animationStep = 0;
        bool finished = false, resetAnimations = false, clearFlag55 = false;
        int requestedBranch = -1;
        std::vector<Command> commands;

    private:
        unsigned segmentEvents = 0;
        bool nextSegment = false;
    };
}
