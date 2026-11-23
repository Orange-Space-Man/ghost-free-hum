#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

namespace gameassetsetup {
    std::filesystem::path prepare(HWND window, bool forceSelect = false, bool forceImport = false);
    bool active();
    bool handleMessage(UINT message, WPARAM key);
    void draw();
    bool multipleEditionsAvailable();
    bool rebootToRegionSelect(bool importAnother = false);
}
