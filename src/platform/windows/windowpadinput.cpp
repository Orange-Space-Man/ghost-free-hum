#include "platform/windows/windowpadinput.h"
#include "platform/windows/controlbindings.h"

#include <windows.h>
#include <Xinput.h>
#include <utility>

playerinput::PadStatus playerinput::translate(const HostPad& pad) {
    PadStatus result;
    const auto axis = [](std::int16_t value, int range) {
        return static_cast<std::int8_t>((int(value) * range) / (value < 0 ? 32768 : 32767));
    };
    result.stickX = axis(pad.stickX, 87);
    result.stickY = axis(pad.stickY, 87);
    result.substickX = axis(pad.substickX, 74);
    result.substickY = axis(pad.substickY, 74);
    const std::pair<unsigned, unsigned> buttons[]{
        {XINPUT_GAMEPAD_DPAD_LEFT, 1}, {XINPUT_GAMEPAD_DPAD_RIGHT, 2},
        {XINPUT_GAMEPAD_DPAD_DOWN, 4}, {XINPUT_GAMEPAD_DPAD_UP, 8},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, 0x40}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, 0x10},
        {XINPUT_GAMEPAD_A, 0x100}, {XINPUT_GAMEPAD_B, 0x200},
        {XINPUT_GAMEPAD_X, 0x400}, {XINPUT_GAMEPAD_Y, 0x800}, {XINPUT_GAMEPAD_START, 0x1000}
    };
    for (const auto& mapping : buttons) if (pad.buttons & mapping.first) result.buttons |= mapping.second;
    result.triggerLeft = pad.triggerLeft;
    result.triggerRight = pad.triggerRight;
    if (pad.triggerLeft >= 250) result.buttons |= 0x40;
    if (pad.triggerRight >= 250) result.buttons |= 0x20;
    result.analogA = (result.buttons & 0x100) ? 255 : 0;
    result.analogB = (result.buttons & 0x200) ? 255 : 0;
    return result;
}

playerinput::HostSample playerinput::pollWindowPad(void* window, bool enabled) {
    if (!enabled || !window || GetForegroundWindow() != static_cast<HWND>(window)) return {};
    return controlbindings::poll(window);
}
