#include "game/input/controllerfeedbackstate.h"

void controllerfeedback::State::update(std::uint32_t frame) {
    const int pulse = pulses.update(frame);
    const int script = scripts.update();
    const int command = combineCommands(pulse, script);
    if (command == '-') {
        if (running) {
            if (motor) motor(MotorCommand::Stop);
            running = false;
        }
        engaged = true;
    }
    else if (command == '*') {
        if (!running) {
            if (motor) motor(MotorCommand::Start);
            running = true;
        }
        engaged = true;
    }
    else if (command == '/' && engaged) stop();
}

void controllerfeedback::State::stop() {
    if (!engaged) return;
    if (motor) motor(MotorCommand::StopHard);
    running = false;
    engaged = false;
}

void controllerfeedback::State::reset() {
    stop();
    running = false;
    engaged = false;
    pulses.reset();
    scripts.reset();
}
