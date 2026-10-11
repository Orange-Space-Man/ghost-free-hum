#include "platform/windows/gameapplication.h"

#include <shellapi.h>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    gameapplication::Application application;
    try {
        std::filesystem::path assets;
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments) {
            const bool valid = count == 1 || (count == 3 && std::wstring(arguments[1]) == L"--assets");
            if (count == 3 && valid) assets = arguments[2];
            LocalFree(arguments);
            if (!valid) throw std::runtime_error("Expected --assets followed by an imported package path.");
        }
        if (assets.empty()) assets = gameassetsetup::prepare();
        if (assets.empty()) return 0;
        application.initialize(instance, assets, true, true);
        return application.run(show);
    } catch (const std::exception& exception) {
        application.close();
        MessageBoxA(nullptr, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
        return 1;
    }
}
