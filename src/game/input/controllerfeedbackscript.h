#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace controllerfeedback {
    struct Script {
        std::uint32_t identity = 0;
        std::string commands;
    };

    struct ScriptSlot {
        bool active = false;
        bool looping = false;
        bool retrigger = false;
        Script script;
        std::uint32_t cursor = 0;
        std::uint32_t length = 0;
        std::uint32_t loopStart = 0;
        std::uint32_t loopEnd = 0;
        std::uint32_t repeats = 0;
    };

    int combineCommands(int first, int second);

    class Scripts {
    public:
        std::array<ScriptSlot, 4> slots;

        bool play(const Script& script);
        int update();
        void reset();
    };
}
