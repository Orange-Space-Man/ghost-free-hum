#pragma once

#include "game/input/controllerfeedbackstate.h"

#include <array>
#include <cstdint>
#include <vector>

namespace playerinput {
    class MotorOutput {
    public:
        virtual ~MotorOutput() = default;
        virtual void command(unsigned port, unsigned command) = 0;
    };

    class WindowsMotorOutput : public MotorOutput {
    public:
        void command(unsigned port, unsigned command) override;
    };

    struct MotorState {
        explicit MotorState(MotorOutput& output);
        void stopHard(int port);
        void start(int port);
        void stop(int port);
        MotorOutput& output;
        std::uint32_t enabled = 0;
        std::array<std::uint8_t, 4> running{};
    };

    struct Pad {
        std::int16_t port = -1;
        std::uint32_t held = 0, pressed = 0, released = 0;
        std::array<float, 2> stick{}, substick{};
        float stickMagnitude = 0, substickMagnitude = 0;
        std::uint8_t analogA = 0, analogB = 0, triggerLeft = 0, triggerRight = 0;
    };

    class ControllerState {
    public:
        ControllerState(MotorState& motors, Pad* pad);
        ~ControllerState();
        ControllerState(const ControllerState&) = delete;
        ControllerState& operator=(const ControllerState&) = delete;
        void reset(Pad* value);
        void stop();
        void pause();
        void update(std::uint32_t frame);
        bool play(const std::vector<std::uint8_t>& executable, unsigned preset);
        Pad* pad;
        controllerfeedback::State feedback;

    private:
        MotorState& motors;
    };
}
