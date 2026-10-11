#include "platform/windows/debugoverlay.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <stdexcept>

#include <windows.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace {
    bool visible = false;
    bool platformReady = false, rendererReady = false;
    std::string systemInfo, sceneInfo;
}

void debugoverlay::open(HWND window, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    visible = false;
    systemInfo = "Windows (version unavailable)";
    using VersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto module = GetModuleHandleW(L"ntdll.dll");
    const auto versionFunction = module ? reinterpret_cast<VersionFunction>(GetProcAddress(module, "RtlGetVersion")) : nullptr;
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (versionFunction && versionFunction(&version) == 0) {
        systemInfo = "Windows " + std::string(version.dwMajorVersion == 10 && version.dwBuildNumber >= 22000 ? "11" :
            version.dwMajorVersion == 10 ? "10" : std::to_string(version.dwMajorVersion) + "." + std::to_string(version.dwMinorVersion));
        systemInfo += " | OS build " + std::to_string(version.dwBuildNumber);
    }
    platformReady = ImGui_ImplWin32_Init(window);
    if (!platformReady) throw std::runtime_error("Could not initialize stats input.");
    rendererReady = ImGui_ImplDX11_Init(device, context);
    if (!rendererReady) throw std::runtime_error("Could not initialize stats rendering.");
}

void debugoverlay::close() {
    if (rendererReady) ImGui_ImplDX11_Shutdown();
    if (platformReady) ImGui_ImplWin32_Shutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    platformReady = rendererReady = visible = false;
}

bool debugoverlay::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!platformReady) return false;
    ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    if (message == WM_KEYDOWN && wParam == VK_F2) {
        if (!(lParam & (1LL << 30))) toggle();
        return true;
    }
    return false;
}

bool debugoverlay::isVisible() { return visible; }
void debugoverlay::toggle() { visible = !visible; }
void debugoverlay::setScene(const std::string& information) { sceneInfo = information; }

void debugoverlay::draw() {
    if (!rendererReady) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (visible) {
        const auto& io = ImGui::GetIO();
        char timing[128];
        std::snprintf(timing, sizeof(timing), "FPS: %.1f | Frame: %.2f ms | %.0fx%.0f | D3D11",
            io.Framerate, io.Framerate > 0 ? 1000.0f / io.Framerate : 0, io.DisplaySize.x, io.DisplaySize.y);
        const std::string text = std::string(timing) + "\n" + systemInfo + " | ghost-free-hum " GHOST_FREE_HUM_VERSION
            " | " GHOST_FREE_HUM_BUILD " x64 | Built " __DATE__ " " __TIME__ "\n" + sceneInfo;
        auto* list = ImGui::GetForegroundDrawList();
        const float wrap = io.DisplaySize.x > 24 ? io.DisplaySize.x - 24 : 1;
        list->AddText(nullptr, 0, {11, 11}, IM_COL32(0, 0, 0, 255), text.c_str(), nullptr, wrap);
        list->AddText(nullptr, 0, {10, 10}, IM_COL32(255, 255, 255, 255), text.c_str(), nullptr, wrap);
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
