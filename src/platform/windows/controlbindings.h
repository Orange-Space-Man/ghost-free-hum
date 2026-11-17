#pragma once

#include "platform/windows/windowpadinput.h"
#include <array>
#include <filesystem>
#include <string>

namespace controlbindings {
    constexpr unsigned count = 20;
    enum class Kind { None, Key, Button, Axis };
    struct Binding {
        Kind kind = Kind::None;
        int code = 0, direction = 1;
    };
    struct Configuration {
        std::array<Binding, count> keyboard{}, controller{};
        int device = -1;
        float deadzone = 0;
    };
    struct Snapshot {
        std::array<bool, 256> keys{};
        std::array<bool, 4> connected{};
        std::array<playerinput::HostPad, 4> pads{};
    };
    const char* targetName(unsigned target);
    Configuration defaults();
    bool valid(const Binding& binding, bool keyboard);
    float strength(const Binding& binding, const Snapshot& snapshot, int device, float deadzone);
    int selectedDevice(const Configuration& config, const Snapshot& snapshot);
    playerinput::HostSample map(const Configuration& config, const Snapshot& snapshot);
    bool detect(const Snapshot& before, const Snapshot& now, bool keyboard, int device, Binding& result);
    std::string name(const Binding& binding);
    bool load(const std::filesystem::path& path, Configuration& config, std::string& error);
    bool save(const std::filesystem::path& path, const Configuration& config, std::string& error);
    Snapshot sample(void* window, bool allKeys = true);
    Configuration& current();
    void initialize(const std::filesystem::path& path);
    bool persist(std::string& error);
    const std::string& loadError();
    void suppressUntilReleased();
    playerinput::HostSample poll(void* window);
}
