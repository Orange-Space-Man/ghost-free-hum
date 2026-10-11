#include "resources/dolresources.h"
#include "game/player/playercontrollerstate.h"
#include "game/startup/startupresources.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Xinput.h>
#include <stdexcept>

void playerinput::WindowsMotorOutput::command(unsigned port, unsigned command) {
    XINPUT_VIBRATION vibration{};
    if (command == 1) vibration.wLeftMotorSpeed = vibration.wRightMotorSpeed = 65535;
    XInputSetState(port, &vibration);
}

playerinput::MotorState::MotorState(MotorOutput& output) : output(output) {}

void playerinput::MotorState::start(int port) {
    if (port < 0 || port >= 4) throw std::runtime_error("Original rumble start requires a valid controller port.");
    if (enabled & (0x80000000u >> port)) {
        output.command(static_cast<unsigned>(port), 1);
        running[port] = 1;
    }
}

void playerinput::MotorState::stop(int port) {
    if (port < 0 || port >= 4) throw std::runtime_error("Original rumble stop requires a valid controller port.");
    if (enabled & (0x80000000u >> port)) {
        output.command(static_cast<unsigned>(port), 0);
        running[port] = 0;
    }
}

void playerinput::MotorState::stopHard(int port) {
    if (port < 0 || port >= 4) throw std::runtime_error("Original rumble stop requires a valid controller port.");
    if (enabled & (0x80000000u >> port)) {
        output.command(static_cast<unsigned>(port), 2);
        running[port] = 0;
    }
}

playerinput::ControllerState::ControllerState(MotorState& motors, Pad* pad) : motors(motors) {
    feedback.motor = [this](controllerfeedback::MotorCommand command) {
        if (!this->pad) return;
        if (command == controllerfeedback::MotorCommand::Start) this->motors.start(this->pad->port);
        else if (command == controllerfeedback::MotorCommand::Stop) this->motors.stop(this->pad->port);
        else this->motors.stopHard(this->pad->port);
    };
    reset(pad);
}

playerinput::ControllerState::~ControllerState() {
    stop();
}

void playerinput::ControllerState::reset(Pad* value) {
    pad = value;
    feedback.running = feedback.engaged = false;
    feedback.pulses.reset();
    feedback.scripts.reset();
}

void playerinput::ControllerState::stop() {
    pause();
    reset(pad);
}

void playerinput::ControllerState::pause() {
    feedback.stop();
}

void playerinput::ControllerState::update(std::uint32_t frame) {
    feedback.update(frame);
}

bool playerinput::ControllerState::play(const std::vector<std::uint8_t>& executable, unsigned preset) {
    const std::uint64_t address = 0x803520bcull + std::uint64_t(preset) * 4;
    if (address > 0xfffffffc) throw std::runtime_error("Controller preset address exceeds original address space.");
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, static_cast<std::uint32_t>(address), 4, bytes, error)) {
        throw std::runtime_error(error);
    }
    controllerfeedback::Script script;
    script.identity = (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
        (std::uint32_t(bytes[2]) << 8) | bytes[3];
    std::uint64_t cursor = script.identity;
    for (;;) {
        if (cursor > 0xffffffff || !dolresources::read(executable, static_cast<std::uint32_t>(cursor), 1, bytes, error)) {
            throw std::runtime_error("Controller preset has no mapped string terminator.");
        }
        if (!bytes[0]) break;
        script.commands.push_back(static_cast<char>(bytes[0]));
        ++cursor;
    }
    return feedback.scripts.play(script);
}
