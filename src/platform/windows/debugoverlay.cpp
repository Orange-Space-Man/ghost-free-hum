#include "platform/windows/debugoverlay.h"
#include "platform/windows/controlsettings.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <algorithm>
#include <stdexcept>

#include <windows.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace {
    bool visible = false, checkpointOpen = false;
    unsigned selectedCheckpoint = 0;
    bool platformReady = false, rendererReady = false;
    std::string systemInfo, sceneInfo, loadingInfo;
}

void debugoverlay::open(HWND window, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    controlsettings::initialize(window);
    visible = false;
    checkpointOpen = false;
    selectedCheckpoint = 0;
    loadingInfo.clear();
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
    platformReady = rendererReady = visible = checkpointOpen = false;
    selectedCheckpoint = 0;
    loadingInfo.clear();
}

bool debugoverlay::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!platformReady) return false;
    ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    if (checkpointOpen && message == WM_KEYDOWN && wParam == VK_ESCAPE) {
        checkpointOpen = false;
        return true;
    }
    if (controlsettings::handleMessage(message, wParam, lParam)) return true;
    if (!controlsettings::visible() && message == WM_KEYDOWN && wParam == VK_F3) {
        if (!(lParam & (1LL << 30))) checkpointOpen = !checkpointOpen;
        return true;
    }
    if (checkpointOpen && (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR ||
        message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_RBUTTONDOWN ||
        message == WM_RBUTTONUP || message == WM_MOUSEWHEEL)) return true;
    if (message == WM_KEYDOWN && wParam == VK_F2) {
        if (!(lParam & (1LL << 30))) toggle();
        return true;
    }
    return false;
}

bool debugoverlay::isVisible() { return visible; }
void debugoverlay::toggle() { visible = !visible; }
void debugoverlay::setScene(const std::string& information) { sceneInfo = information; }

void debugoverlay::setLoading(const std::string& information) { loadingInfo = information; }
bool debugoverlay::isLoading() { return !loadingInfo.empty(); }
bool debugoverlay::checkpointVisible() { return checkpointOpen; }
unsigned debugoverlay::takeCheckpoint() {
    const auto result = selectedCheckpoint;
    selectedCheckpoint = 0;
    return result;
}

void debugoverlay::draw() {
    if (!rendererReady) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (!loadingInfo.empty()) {
        auto* list = ImGui::GetForegroundDrawList();
        const auto width = ImGui::GetIO().DisplaySize.x;
        list->AddText(nullptr, 0, {17, 17}, IM_COL32(0, 0, 0, 255), loadingInfo.c_str(), nullptr, width > 32 ? width - 32 : 1);
        list->AddText(nullptr, 0, {16, 16}, IM_COL32(255, 255, 255, 255), loadingInfo.c_str(), nullptr, width > 32 ? width - 32 : 1);
    } else if (visible) {
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
    if (checkpointOpen && loadingInfo.empty()) {
        const auto display = ImGui::GetIO().DisplaySize;
        const ImVec2 size{std::min(display.x - 24.0f, 360.0f), 0};
        ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.32f, 0.19f, 1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.23f, 0.44f, 0.27f, 1});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.30f, 0.52f, 0.34f, 1});
        if (ImGui::Begin("Go to", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            constexpr const char* destinations[]{"Mansion foyer", "Foyer after first key", "Training room", "Gallery"};
            for (unsigned i = 0; i < 4; ++i) {
                if (ImGui::Button(destinations[i], {ImGui::GetContentRegionAvail().x, 36})) {
                    selectedCheckpoint = i + 1;
                    checkpointOpen = false;
                }
            }
            ImGui::TextDisabled("F3 or Esc to close");
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
    }
    controlsettings::draw();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

