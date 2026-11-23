#include "platform/d3d11/enhancementsettings.h"

#include <atomic>
#include <fstream>
#include <system_error>
#include <windows.h>

namespace {
    std::filesystem::path settingsPath;
    std::atomic<bool> copyFilterDisabled{false};
    std::atomic<bool> fullMapRotationEnabled{false};
    std::atomic<bool> automaticKeyChestsEnabled{false};
    std::string error;
}

void enhancementsettings::initialize(const std::filesystem::path& path) {
    settingsPath = path;
    error.clear();
    copyFilterDisabled = false;
    fullMapRotationEnabled = false;
    automaticKeyChestsEnabled = false;
    std::ifstream file(path);
    if (!file) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line == "disableCopyFilter=1") copyFilterDisabled = true;
        else if (line == "disableCopyFilter=0") copyFilterDisabled = false;
        else if (line == "rotateMap360=1") fullMapRotationEnabled = true;
        else if (line == "rotateMap360=0") fullMapRotationEnabled = false;
        else if (line == "automaticallyOpenKeyChests=1") automaticKeyChestsEnabled = true;
        else if (line == "automaticallyOpenKeyChests=0") automaticKeyChestsEnabled = false;
    }
}

bool enhancementsettings::persist(std::string& message) {
    if (settingsPath.empty()) { message = "Enhancement settings path is not set."; return false; }
    std::error_code code;
    std::filesystem::create_directories(settingsPath.parent_path(), code);
    const auto temporary = std::filesystem::path(settingsPath).concat(".tmp");
    {
        std::ofstream file(temporary, std::ios::trunc);
        file << "disableCopyFilter=" << (copyFilterDisabled ? 1 : 0) << "\n";
        file << "rotateMap360=" << (fullMapRotationEnabled ? 1 : 0) << "\n";
        file << "automaticallyOpenKeyChests=" << (automaticKeyChestsEnabled ? 1 : 0) << "\n";
        if (!file) { message = "Could not write " + temporary.string(); return false; }
    }
    if (!MoveFileExW(temporary.c_str(), settingsPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        code = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        message = "Could not save " + settingsPath.string() + ": " + code.message();
        return false;
    }
    return true;
}

const std::string& enhancementsettings::loadError() { return error; }
bool enhancementsettings::disableCopyFilter() { return copyFilterDisabled; }
void enhancementsettings::setDisableCopyFilter(bool value) { copyFilterDisabled = value; }
bool enhancementsettings::rotateMap360() { return fullMapRotationEnabled; }
void enhancementsettings::setRotateMap360(bool value) { fullMapRotationEnabled = value; }
bool enhancementsettings::automaticallyOpenKeyChests() { return automaticKeyChestsEnabled; }
void enhancementsettings::setAutomaticallyOpenKeyChests(bool value) { automaticKeyChestsEnabled = value; }
