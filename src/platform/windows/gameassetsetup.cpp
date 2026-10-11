#include "platform/windows/gameassetsetup.h"
#include "resources/gameassetpackage.h"

#include <windows.h>
#include <commdlg.h>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>

namespace {
    std::atomic<bool> cancelled{false};

    LRESULT CALLBACK importProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_CLOSE) {
            cancelled = true;
            SetWindowTextW(window, L"ghost-free-hum | Cancelling import...");
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

std::filesystem::path gameassetsetup::prepare() {
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768)) throw std::runtime_error("Cannot locate ghost-free-hum.");
    const auto directory = std::filesystem::path(executable).parent_path();
    const auto destination = directory / "assets" / "glme01.ppak";
    if (std::filesystem::exists(destination)) {
        gameassets::Package package;
        std::string error;
        if (!package.open(destination, error)) throw std::runtime_error(error + " Move assets/glme01.ppak aside and launch ghost-free-hum again.");
        return destination;
    }
    wchar_t filename[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = L"GameCube ISO (*.iso)\0*.iso\0\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = 32768;
    dialog.lpstrTitle = L"Import Luigi's Mansion (USA revision 0)";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return {};

    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = importProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"GhostFreeHumAssetImport";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot create the import window.");
    HWND window = CreateWindowW(windowClass.lpszClassName, L"ghost-free-hum | Importing game assets",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 480, 150,
        nullptr, nullptr, instance, nullptr);
    if (!window) throw std::runtime_error("Cannot create the import window.");
    HWND label = CreateWindowW(L"STATIC", L"Preparing import...", WS_CHILD | WS_VISIBLE,
        24, 24, 425, 60, window, nullptr, instance, nullptr);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    cancelled = false;
    std::atomic<unsigned> percent{0};
    std::string error;
    bool success = false;
    try {
        auto task = std::async(std::launch::async, [&] {
            return gameassets::importDisc(filename, destination, error, [&](std::uint64_t done, std::uint64_t total) {
                percent = total == 0 ? 100 : static_cast<unsigned>(done * 100 / total);
                return !cancelled.load();
            });
        });
        while (task.wait_for(std::chrono::milliseconds(30)) != std::future_status::ready) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            const auto text = L"Importing game assets: " + std::to_wstring(percent.load()) + L"%\nThe ISO is only needed for this import.";
            SetWindowTextW(label, text.c_str());
        }
        success = task.get();
    } catch (...) {
        DestroyWindow(window);
        throw;
    }
    DestroyWindow(window);
    if (!success) {
        if (cancelled) return {};
        throw std::runtime_error(error);
    }
    return destination;
}
