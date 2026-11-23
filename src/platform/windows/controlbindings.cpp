#include "platform/windows/controlbindings.h"
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
    const char* names[] = {"A", "B", "X", "Y", "Z", "Start / Pause", "L trigger", "R trigger",
        "D-pad left", "D-pad right", "D-pad down", "D-pad up", "Stick left", "Stick right", "Stick down", "Stick up",
        "C-stick left", "C-stick right", "C-stick down", "C-stick up"};
    const unsigned masks[] = {0x100, 0x200, 0x400, 0x800, 0x10, 0x1000, 0x40, 0x20, 1, 2, 4, 8};
    controlbindings::Configuration settings = controlbindings::defaults();
    std::filesystem::path settingsPath;
    std::string initializationError;
    bool suppressed = false;

    float axis(const playerinput::HostPad& pad, int index) {
        if (index >= 4) return (index == 4 ? pad.triggerLeft : pad.triggerRight) / 255.0f;
        const int value = index == 0 ? pad.stickX : index == 1 ? pad.stickY : index == 2 ? pad.substickX : pad.substickY;
        return value / (value < 0 ? 32768.0f : 32767.0f);
    }
}

const char* controlbindings::targetName(unsigned target) { return names[target]; }

controlbindings::Configuration controlbindings::defaults() {
    Configuration result;
    const int keys[count] = {'A', 'B', 'X', 'Y', 'Z', VK_RETURN, 'Q', 'E', 0, 0, 0, 0,
        VK_LEFT, VK_RIGHT, VK_DOWN, VK_UP, 'J', 'L', 'K', 'I'};
    const int buttons[] = {XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_START, 0, 0,
        XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_UP};
    for (unsigned i = 0; i < count; ++i) {
        if (keys[i]) result.keyboard[i] = {Kind::Key, keys[i]};
        if (i < 12 && buttons[i]) result.controller[i] = {Kind::Button, buttons[i]};
        if (i >= 12) result.controller[i] = {Kind::Axis, int((i - 12) / 2), i % 2 ? 1 : -1};
    }
    result.controller[6] = {Kind::Axis, 4};
    result.controller[7] = {Kind::Axis, 5};
    return result;
}

bool controlbindings::valid(const Binding& binding, bool keyboard) {
    if (binding.kind == Kind::None) return true;
    if (keyboard) return binding.kind == Kind::Key && binding.code >= 8 && binding.code < 256 &&
        binding.code != VK_ESCAPE && binding.code != VK_F1 && binding.code != VK_F2 &&
        binding.code != VK_F5;
    if (binding.kind == Kind::Axis) return binding.code >= 0 && binding.code < 6 &&
        (binding.direction == 1 || (binding.code < 4 && binding.direction == -1));
    return binding.kind == Kind::Button && binding.code > 0 && !(binding.code & (binding.code - 1)) &&
        !(binding.code & ~0xf3ff);
}

int controlbindings::selectedDevice(const Configuration& config, const Snapshot& snapshot) {
    if (config.device == -2) return -1;
    if (config.device >= 0) return snapshot.connected[config.device] ? config.device : -1;
    for (int i = 0; i < 4; ++i) if (snapshot.connected[i]) return i;
    return -1;
}

float controlbindings::strength(const Binding& binding, const Snapshot& snapshot, int device, float deadzone) {
    if (binding.kind == Kind::Key) return snapshot.keys[binding.code] ? 1.0f : 0.0f;
    if (device < 0 || !snapshot.connected[device]) return 0;
    if (binding.kind == Kind::Button) return snapshot.pads[device].buttons & binding.code ? 1.0f : 0.0f;
    if (binding.kind != Kind::Axis) return 0;
    const float value = std::max(0.0f, axis(snapshot.pads[device], binding.code) * binding.direction);
    if (binding.code >= 4) return value;
    return std::max(0.0f, (value - deadzone) / (1 - deadzone));
}

playerinput::HostSample controlbindings::map(const Configuration& config, const Snapshot& snapshot) {
    playerinput::HostSample result;
    const int device = selectedDevice(config, snapshot);
    result.port = static_cast<std::int16_t>(device);
    std::array<float, count> values{};
    for (unsigned i = 0; i < count; ++i) values[i] = std::max(strength(config.keyboard[i], snapshot, -1, 0),
        strength(config.controller[i], snapshot, device, config.deadzone));
    for (unsigned i = 0; i < 12; ++i) if (values[i] >= (i == 6 || i == 7 ? 250.0f / 255 : 0.5f)) result.status.buttons |= masks[i];
    result.status.triggerLeft = static_cast<std::uint8_t>(std::lround(values[6] * 255));
    result.status.triggerRight = static_cast<std::uint8_t>(std::lround(values[7] * 255));
    result.status.stickX = static_cast<std::int8_t>((values[13] - values[12]) * 87);
    result.status.stickY = static_cast<std::int8_t>((values[15] - values[14]) * 87);
    result.status.substickX = static_cast<std::int8_t>((values[17] - values[16]) * 74);
    result.status.substickY = static_cast<std::int8_t>((values[19] - values[18]) * 74);
    result.status.analogA = result.status.buttons & 0x100 ? 255 : 0;
    result.status.analogB = result.status.buttons & 0x200 ? 255 : 0;
    return result;
}

bool controlbindings::detect(const Snapshot& before, const Snapshot& now, bool keyboard, int device, Binding& result) {
    if (keyboard) {
        for (int key = 8; key < 256; ++key) if (now.keys[key] && !before.keys[key] && valid({Kind::Key, key}, true)) {
            result = {Kind::Key, key}; return true;
        }
    } else if (device >= 0 && now.connected[device] && before.connected[device]) {
        for (int bit = 0; bit < 16; ++bit) {
            Binding candidate{Kind::Button, 1 << bit};
            if (valid(candidate, false) && strength(candidate, now, device, 0) && !strength(candidate, before, device, 0)) {
                result = candidate; return true;
            }
        }
        for (int index = 0; index < 6; ++index) for (int sign : {1, -1}) {
            Binding candidate{Kind::Axis, index, sign};
            if (valid(candidate, false) && strength(candidate, now, device, 0) > 0.65f && strength(candidate, before, device, 0) <= 0.65f) {
                result = candidate; return true;
            }
        }
    }
    return false;
}

std::string controlbindings::name(const Binding& binding) {
    if (binding.kind == Kind::None) return "Not assigned";
    if (binding.kind == Kind::Key) {
        wchar_t text[64]{};
        UINT scan = MapVirtualKeyW(binding.code, MAPVK_VK_TO_VSC);
        if (binding.code >= VK_PRIOR && binding.code <= VK_DOWN) scan |= 0x100;
        if (GetKeyNameTextW(scan << 16, text, 64)) {
            char utf8[192]{}; WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, 192, nullptr, nullptr); return utf8;
        }
        return "Key " + std::to_string(binding.code);
    }
    if (binding.kind == Kind::Axis) {
        const char* axes[] = {"Left stick X", "Left stick Y", "Right stick X", "Right stick Y", "LT", "RT"};
        return std::string(axes[binding.code]) + (binding.code < 4 ? (binding.direction > 0 ? " +" : " -") : "");
    }
    const char* labels[] = {"D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Back", "Left stick click", "Right stick click", "LB", "RB", "", "", "A", "B", "X", "Y"};
    for (unsigned i = 0; i < 16; ++i) if (binding.code == (1 << i)) return labels[i];
    return "Unknown";
}

bool controlbindings::load(const std::filesystem::path& path, Configuration& config, std::string& error) {
    error.clear();
    std::ifstream file(path);
    if (!file) { error = "Could not read control settings."; return false; }
    std::string magic; int version = 0;
    Configuration candidate;
    if (!(file >> magic >> version >> candidate.device >> candidate.deadzone) || magic != "LMD-CONTROLS" || version != 1 ||
        candidate.device < -2 || candidate.device > 3 || !std::isfinite(candidate.deadzone) || candidate.deadzone < 0 || candidate.deadzone > 0.5f) {
        error = "Invalid control settings; defaults are active."; return false;
    }
    for (unsigned source = 0; source < 2; ++source) for (unsigned i = 0; i < count; ++i) {
        auto& binding = source ? candidate.controller[i] : candidate.keyboard[i];
        int kind;
        if (!(file >> kind >> binding.code >> binding.direction)) { error = "Incomplete control settings; defaults are active."; return false; }
        binding.kind = static_cast<Kind>(kind);
        if (!valid(binding, source == 0)) { error = "Invalid binding; defaults are active."; return false; }
    }
    config = candidate;
    return true;
}

bool controlbindings::save(const std::filesystem::path& path, const Configuration& config, std::string& error) {
    error.clear();
    try {
        if (path.empty()) throw std::runtime_error("Local settings folder is unavailable. Changes remain active for this session.");
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        auto temporary = path; temporary += L".tmp";
        std::ofstream file(temporary, std::ios::trunc);
        file << "LMD-CONTROLS 1\n" << config.device << ' ' << config.deadzone << '\n';
        for (const auto* bindings : {&config.keyboard, &config.controller}) for (const auto& binding : *bindings)
            file << int(binding.kind) << ' ' << binding.code << ' ' << binding.direction << '\n';
        file.close();
        if (!file || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not save controls. Changes remain active for this session.");
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

controlbindings::Snapshot controlbindings::sample(void* window, bool allKeys) {
    Snapshot result;
    const bool focused = window && GetForegroundWindow() == static_cast<HWND>(window);
    static std::array<bool, 4> connected{};
    static ULONGLONG lastProbe = 0;
    const auto now = GetTickCount64();
    const bool probe = !lastProbe || now - lastProbe >= 1000;
    if (probe) lastProbe = now;
    for (int i = 0; i < 4; ++i) {
        if (!connected[i] && !probe) continue;
        XINPUT_STATE pad{};
        result.connected[i] = connected[i] = XInputGetState(i, &pad) == ERROR_SUCCESS;
        if (result.connected[i] && focused) {
            const auto& p = pad.Gamepad;
            result.pads[i] = {p.wButtons, p.sThumbLX, p.sThumbLY, p.sThumbRX, p.sThumbRY, p.bLeftTrigger, p.bRightTrigger};
        }
    }
    if (focused) {
        if (allKeys) {
            for (int key = 8; key < 256; ++key) result.keys[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
        } else {
            for (const auto& binding : settings.keyboard) if (binding.kind == Kind::Key)
                result.keys[binding.code] = (GetAsyncKeyState(binding.code) & 0x8000) != 0;
        }
    }
    return result;
}

controlbindings::Configuration& controlbindings::current() { return settings; }
const std::string& controlbindings::loadError() { return initializationError; }
void controlbindings::initialize(const std::filesystem::path& path) {
    settingsPath = path; settings = defaults(); initializationError.clear();
    std::error_code error;
    if (std::filesystem::exists(path, error)) load(path, settings, initializationError);
}
bool controlbindings::persist(std::string& error) { return save(settingsPath, settings, error); }
void controlbindings::suppressUntilReleased() { suppressed = true; }
playerinput::HostSample controlbindings::poll(void* window) {
    const auto snapshot = sample(window, false);
    auto result = map(settings, snapshot);
    if (suppressed) {
        const auto& pad = result.status;
        if (!pad.buttons && !pad.triggerLeft && !pad.triggerRight && std::abs(pad.stickX) < 20 && std::abs(pad.stickY) < 20 &&
            std::abs(pad.substickX) < 20 && std::abs(pad.substickY) < 20) suppressed = false;
        return {};
    }
    return result;
}
