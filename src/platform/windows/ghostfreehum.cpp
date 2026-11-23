#include "platform/windows/gameapplication.h"

#include <shellapi.h>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    gameapplication::Application application;
    try {
        std::filesystem::path assets;
        bool forceSelect = false, forceImport = false;
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments) {
            const bool valid = count == 1 || (count == 3 && std::wstring(arguments[1]) == L"--assets") ||
                (count == 2 && (std::wstring(arguments[1]) == L"--region-select" || std::wstring(arguments[1]) == L"--import"));
            if (count == 3 && valid) assets = arguments[2];
            if (count == 2 && valid) {
                forceSelect = std::wstring(arguments[1]) == L"--region-select";
                forceImport = std::wstring(arguments[1]) == L"--import";
            }
            LocalFree(arguments);
            if (!valid) throw std::runtime_error("Expected --assets <package>, --region-select, or --import.");
        }
        if (assets.empty()) {
            const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            auto window = application.initializeSetup(instance, show);
            assets = gameassetsetup::prepare(window, forceSelect || shift, forceImport);
        }
        if (assets.empty()) return 0;
        application.initialize(instance, assets, true, true);
        return application.run(show);
    } catch (const std::exception& exception) {
        application.close();
        MessageBoxA(nullptr, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
        return 1;
    }
}
