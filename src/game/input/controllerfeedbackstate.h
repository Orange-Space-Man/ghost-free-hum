#pragma once

#include "game/input/controllerfeedbackpulse.h"
#include "game/input/controllerfeedbackscript.h"

#include <functional>

namespace controllerfeedback {
    enum class MotorCommand { Start, Stop, StopHard };

    class State {
    public:
        Pulses pulses;
        Scripts scripts;
        bool running = false;
        bool engaged = false;
        std::function<void(MotorCommand)> motor;

        void update(std::uint32_t frame);
        void stop();
        void reset();
    };
}
