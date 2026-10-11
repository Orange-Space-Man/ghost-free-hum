#include "game/input/controllerfeedbackscript.h"
#include "game/input/controllerfeedbackpulse.h"
#include "game/input/controllerfeedbackstate.h"
#include "game/player/playercontrollerstate.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main() {
    try {
        controllerfeedback::Scripts scripts;
        struct Output : playerinput::MotorOutput {
            std::vector<unsigned> commands;
            void command(unsigned port, unsigned value) override {
                require(port == 2, "Bound feedback must use its original controller port.");
                commands.push_back(value);
            }
        } output;
        playerinput::MotorState motors(output);
        playerinput::Pad pad{2};
        {
            playerinput::ControllerState controller(motors, &pad);
            motors.enabled = 0x20000000;
            controller.feedback.scripts.play({9, "*-*/"});
            controller.update(0);
            require(motors.running[2] == 1, "Controller update must deliver start through the enabled motor mask.");
            controller.pause();
            require(controller.feedback.scripts.slots[0].active, "Manager pause must retain script ownership.");
            controller.update(1);
            controller.update(2);
            require(output.commands == std::vector<unsigned>{1, 2, 1}, "Paused script must resume from its retained cursor.");
            controller.stop();
            require(output.commands.back() == 2 && !controller.feedback.scripts.slots[0].active,
                "Owner cleanup must stop hardware and release scheduled feedback.");
            motors.enabled = 0;
            const auto count = output.commands.size();
            controller.feedback.scripts.play({10, "*-/"});
            for (unsigned i = 0; i < 4; ++i) controller.update(i);
            require(output.commands.size() == count, "Disabled motor must still advance feedback without hardware output.");
            controller.reset(nullptr);
            controller.feedback.pulses.play(1, 2, 0);
            controller.update(0);
            require(controller.feedback.running && output.commands.size() == count,
                "Absent pad must preserve internal feedback transitions without output.");
        }
        controllerfeedback::State feedback;
        std::vector<controllerfeedback::MotorCommand> commands;
        feedback.motor = [&](auto command) { commands.push_back(command); };
        feedback.scripts.play({1, "**--/"});
        for (unsigned i = 0; i < 6; ++i) feedback.update(i);
        require(commands == std::vector<controllerfeedback::MotorCommand>{controllerfeedback::MotorCommand::Start,
            controllerfeedback::MotorCommand::Stop, controllerfeedback::MotorCommand::StopHard},
            "Motor commands must fire only on original state transitions.");
        feedback.pulses.play(1, 3, 0);
        feedback.scripts.play({2, "---"});
        feedback.update(0);
        require(feedback.running, "Pulse start must win over scripted soft stop.");
        feedback.stop();
        require(!feedback.engaged && feedback.pulses.slots[0].active, "Hard stop must retain scheduled feedback.");
        feedback.update(1);
        require(feedback.running, "Retained feedback must resume on the next update.");
        feedback.reset();
        require(!feedback.engaged && !feedback.pulses.slots[0].active && !feedback.scripts.slots[0].active,
            "Reset must stop the motor and release both pools.");
        controllerfeedback::Pulses pulses;
        pulses.play(0.5, 16, 0);
        for (unsigned frame = 0; frame < 16; ++frame) require(pulses.update(frame) == (frame % 2 ? '-' : '*'),
            "Half amplitude must use the original bit-reversed sixteen-frame motor schedule.");
        require(!pulses.slots[0].active && pulses.update(0) == '/', "Pulse must retire after its final active frame.");
        pulses.play(1, 0, 2);
        require(pulses.update(0) == '*' && pulses.slots[0].fadeRemaining == 1 && pulses.update(1) == '-' && !pulses.slots[0].active,
            "Fade must use the squared remaining fraction before decrementing.");
        pulses.play(1, 0, 0);
        require(pulses.update(0) == '/' && pulses.slots[0].active, "Original zero-duration pulse retains its occupied slot.");
        pulses.reset();
        for (unsigned i = 0; i < 4; ++i) pulses.play(0.25, i + 1, 0);
        pulses.play(0.75, 9, 0);
        require(pulses.slots[3].remaining == 9 && pulses.slots[0].remaining == 1, "Full pulse pool replaces the longest remaining pulse.");
        pulses.reset();
        pulses.play(1, 0xffffffff, 0);
        require(pulses.slots[0].remaining == 0xffffffff, "Unsigned duration round-up must preserve conversion saturation.");
        require(!scripts.play({1, "0*"}) && !scripts.play({1, "00*1"}), "Unpaired or duplicate loop markers must fail.");
        require(scripts.play({1, "*-"}), "One-shot must allocate.");
        require(scripts.update() == '*' && scripts.update() == '-' && scripts.update() == '/' && !scripts.slots[0].active,
            "One-shot must consume exactly one motor command per update, then retire at its terminator.");
        require(scripts.play({2, "0*-1/"}), "Loop must allocate.");
        require(scripts.update() == '*', "Loop marker must not consume a frame.");
        require(!scripts.play({2, "0*-1/"}) && scripts.slots[0].retrigger, "Retrigger must retain identity and return false.");
        require(scripts.update() == '-', "Retrigger must preserve the cursor.");
        require(!scripts.play({2, "0*-1/"}) && scripts.update() == '*' && scripts.slots[0].repeats == 1,
            "End marker must loop during the same update.");
        require(scripts.update() == '/' && !scripts.slots[0].looping, "Unrefreshed loop must jump directly to its tail.");
        scripts.update();
        require(!scripts.slots[0].active, "Loop tail must retire.");
        scripts.reset();
        for (unsigned i = 0; i < 4; ++i) require(scripts.play({10 + i, "*"}), "All four slots must be available.");
        require(!scripts.play({20, "-"}), "Full one-shot pool must reject replacement.");
        require(scripts.update() == '*', "Overlapping starts must retain priority.");
        scripts.reset();
        for (unsigned i = 0; i < 4; ++i) require(scripts.play({30 + i, "0*1/"}), "Loop slots must allocate.");
        require(scripts.play({40, "-"}) && scripts.slots[0].script.identity == 40,
            "Equal remaining durations must replace the earliest loop.");
        require(controllerfeedback::combineCommands('-', '/') == '-' && controllerfeedback::combineCommands('-', '*') == '*' &&
            controllerfeedback::combineCommands('*', '-') == '*' && controllerfeedback::combineCommands('.', '*') == '.',
            "Motor command arbitration must retain original priority.");
        std::cout << "Controller feedback checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
