#include "platform/windows/controlsettings.h"
#include "platform/windows/controlbindings.h"
#include "platform/windows/debugoverlay.h"
#include "platform/windows/gameassetsetup.h"
#include "platform/d3d11/enhancementsettings.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    HWND host = nullptr;
    bool opened = false;
    unsigned selected = 0;
    unsigned enhancementPage = 0;
    unsigned debugPage = 0;
    unsigned settingsPage = 0;
    int listening = -1;
    double listenUntil = 0;
    controlbindings::Snapshot previous;
    std::string notice, enhancementNotice;
    bool saveFailed = false;
    controlsettings::ParticleDebug debugParticles;
    const ImVec4 green{0.51f, 0.85f, 0.35f, 1}, muted{0.56f, 0.65f, 0.59f, 1};

    void save() {
        std::string error;
        saveFailed = !controlbindings::persist(error);
        notice = saveFailed ? error : "Changes saved";
    }

    void saveEnhancements() {
        std::string saveError;
        enhancementNotice = enhancementsettings::persist(saveError) ? "Changes saved" : saveError;
    }

    bool same(const controlbindings::Binding& a, const controlbindings::Binding& b) {
        return a.kind == b.kind && a.code == b.code && a.direction == b.direction;
    }

    void assign(const controlbindings::Binding& binding) {
        auto& config = controlbindings::current();
        auto& mappings = listening == 0 ? config.keyboard : config.controller;
        int swapped = -1;
        for (unsigned i = 0; i < mappings.size(); ++i) if (i != selected && same(mappings[i], binding)) {
            mappings[i] = mappings[selected]; swapped = int(i); break;
        }
        mappings[selected] = binding;
        listening = -1;
        save();
        if (!saveFailed && swapped >= 0) notice = std::string("Swapped with ") + controlbindings::targetName(swapped) + " - saved";
    }



    void bindingCell(unsigned target, int source, const controlbindings::Snapshot& snapshot) {
        auto& config = controlbindings::current();
        auto& binding = source == 0 ? config.keyboard[target] : config.controller[target];
        ImGui::PushID(int(target * 2 + source));
        const bool capturing = selected == target && listening == source;
        const int device = controlbindings::selectedDevice(config, snapshot);
        const bool active = controlbindings::strength(binding, snapshot, source == 0 ? -1 : device, config.deadzone) > 0.15f;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.42f, 0.22f, 1));
        const auto caption = capturing ? std::string("Press an input...") : controlbindings::name(binding);
        const float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(caption.c_str(), {std::max(20.0f, width - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x), 0})) {
            selected = target; listening = source; previous = snapshot; listenUntil = ImGui::GetTime() + 12;
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("x", {ImGui::GetFrameHeight(), 0})) { binding = {}; listening = -1; save(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear binding");
        ImGui::PopID();
    }

}

void controlsettings::initialize(HWND window) {
    host = window; opened = false; listening = -1; debugParticles = {};
    wchar_t path[32768]{};
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, 32768);
    if (length && length < 32768) {
        controlbindings::initialize(std::filesystem::path(path) / L"ghost-free-hum" / L"controls.ini");
        enhancementsettings::initialize(std::filesystem::path(path) / L"ghost-free-hum" / L"enhancements.ini");
    }
    notice = controlbindings::loadError();
    saveFailed = !notice.empty();

}

const controlsettings::ParticleDebug& controlsettings::particleDebug() { return debugParticles; }

bool controlsettings::visible() { return opened; }
void controlsettings::setVisible(bool value) {
    opened = value; listening = -1;
    previous = controlbindings::sample(host);
    if (!value) controlbindings::suppressUntilReleased();
}

bool controlsettings::handleMessage(UINT message, WPARAM key, LPARAM flags) {
    if (message == WM_KEYDOWN && key == VK_ESCAPE) {
        if (!(flags & (1LL << 30))) {
            if (listening >= 0) listening = -1;
            else setVisible(!opened);
        }
        return true;
    }
    return opened && (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR ||
        message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_RBUTTONDOWN || message == WM_RBUTTONUP || message == WM_MOUSEWHEEL);
}

void controlsettings::draw() {
    if (!opened) return;
    const auto snapshot = controlbindings::sample(host);
    if (listening >= 0) {
        controlbindings::Binding binding;
        if (controlbindings::detect(previous, snapshot, listening == 0,
            controlbindings::selectedDevice(controlbindings::current(), snapshot), binding)) assign(binding);
        else if (ImGui::GetTime() > listenUntil) { listening = -1; notice = "No input detected. Click a binding to try again."; }
    }
    previous = snapshot;
    const auto savedStyle = ImGui::GetStyle();
    ImGui::GetStyle() = ImGuiStyle();
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_CheckMark] = green;
    style.Colors[ImGuiCol_SliderGrab] = {0.28f, 0.58f, 0.32f, 1};
    style.Colors[ImGuiCol_SliderGrabActive] = green;
    for (auto color : {ImGuiCol_Button, ImGuiCol_Header, ImGuiCol_FrameBg, ImGuiCol_Tab})
        style.Colors[color] = {0.16f, 0.28f, 0.19f, 1};
    for (auto color : {ImGuiCol_ButtonHovered, ImGuiCol_HeaderHovered, ImGuiCol_FrameBgHovered, ImGuiCol_TabHovered})
        style.Colors[color] = {0.24f, 0.43f, 0.28f, 1};
    for (auto color : {ImGuiCol_ButtonActive, ImGuiCol_HeaderActive, ImGuiCol_FrameBgActive, ImGuiCol_TabSelected})
        style.Colors[color] = {0.21f, 0.38f, 0.24f, 1};
    style.Colors[ImGuiCol_TabSelectedOverline] = green;
    const auto display = ImGui::GetIO().DisplaySize;
    const ImVec2 size = display;
    ImGui::PushFont(nullptr, 16);
    ImGui::SetNextWindowPos({(display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f});
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Controls settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::BeginTabBar("settingsTabs")) {
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::BeginChild("navigation", {105, 0}, ImGuiChildFlags_Borders);
            if (ImGui::Selectable("Controls", settingsPage == 0)) settingsPage = 0;
            if (ImGui::Selectable("Misc", settingsPage == 1)) settingsPage = 1;
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("content", {0, 0});
            if (settingsPage == 1) {
                ImGui::SeparatorText("Game edition");
                ImGui::BeginDisabled(!gameassetsetup::multipleEditionsAvailable());
                if (ImGui::Button("Reboot to region select")) {
                    if (gameassetsetup::rebootToRegionSelect()) PostMessageW(host, WM_CLOSE, 0, 0);
                    else notice = "Could not restart the game.";
                }
                ImGui::EndDisabled();
                if (ImGui::Button("Import another edition")) {
                    if (gameassetsetup::rebootToRegionSelect(true)) PostMessageW(host, WM_CLOSE, 0, 0);
                    else notice = "Could not restart the importer.";
                }
                if (!notice.empty()) ImGui::TextUnformatted(notice.c_str());
                if (ImGui::Button("Back to game")) setVisible(false);
            } else {
            auto& config = controlbindings::current();
            const int connected = controlbindings::selectedDevice(config, snapshot);
            const auto deviceLabel = [&](int device) {
                if (device == -2) return std::string("Keyboard only");
                if (device == -1) return std::string("Keyboard + automatic controller");
                return "Keyboard + XInput " + std::to_string(device + 1) + (snapshot.connected[device] ? " / Connected" : " / Disconnected");
            };
            ImGui::TextUnformatted("Input source");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##source", deviceLabel(config.device).c_str())) {
                for (int device : {-1, -2, 0, 1, 2, 3}) if (ImGui::Selectable(deviceLabel(device).c_str(), config.device == device)) {
                    config.device = device; listening = -1; save();
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("%s", config.device == -2 ? "Keyboard only" : connected >= 0 ? "Controller connected" : "No controller connected");
            const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 3 + ImGui::GetStyle().ItemSpacing.y;
            if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV,
                {0, std::max(80.0f, ImGui::GetContentRegionAvail().y - footerHeight)})) {
                ImGui::TableSetupColumn("GameCube input", ImGuiTableColumnFlags_WidthStretch, 0.85f);
                ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthStretch, 1.1f);
                ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthStretch, 1.1f);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                for (unsigned target = 0; target < 20; ++target) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(controlbindings::targetName(target));
                    ImGui::TableSetColumnIndex(1); bindingCell(target, 0, snapshot);
                    ImGui::TableSetColumnIndex(2); bindingCell(target, 1, snapshot);
                }
                ImGui::EndTable();
            }
            ImGui::SetNextItemWidth(140);
            float percent = config.deadzone * 100;
            if (ImGui::SliderFloat("Stick deadzone", &percent, 0, 50, "%.0f%%")) { config.deadzone = percent / 100; save(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0%% preserves original input response. Increase to counter stick drift.");
            if (listening >= 0) {
                ImGui::TextColored(green, "%s", listening == 1 && connected < 0 ? "Connect a controller. Esc cancels." : "Press an input. Esc cancels.");
            } else if (!notice.empty()) {
                ImGui::TextUnformatted(notice.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", notice.c_str());
            } else {
                ImGui::TextDisabled("Click to rebind. Changes save automatically.");
            }
            if (ImGui::Button("Reset defaults")) { config = controlbindings::defaults(); listening = -1; save(); }
            ImGui::SameLine();
            if (ImGui::Button("Back to game")) setVisible(false);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Enhancements")) {
            ImGui::BeginChild("enhancementNavigation", {105, 0}, ImGuiChildFlags_Borders);
            if (ImGui::Selectable("Graphics", enhancementPage == 0)) enhancementPage = 0;
            if (ImGui::Selectable("Gameplay", enhancementPage == 1)) enhancementPage = 1;
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("enhancementContent", {0, 0});
            if (enhancementPage == 0) {
                bool disableCopyFilter = enhancementsettings::disableCopyFilter();
                if (ImGui::Checkbox("Disable Copy Filter", &disableCopyFilter)) {
                    enhancementsettings::setDisableCopyFilter(disableCopyFilter);
                    saveEnhancements();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Removes the vertical depth-copy filter and Luigi's self-shadow blotches. Off keeps the original hardware appearance.");
            } else {
                ImGui::SeparatorText("GameBoy Horror");
                bool fullRotation = enhancementsettings::rotateMap360();
                if (ImGui::Checkbox("Rotate map 360 degrees", &fullRotation)) {
                    enhancementsettings::setRotateMap360(fullRotation);
                    saveEnhancements();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Removes the original map's left and right rotation limits.");
                ImGui::SeparatorText("World");
                bool automaticKeyChests = enhancementsettings::automaticallyOpenKeyChests();
                if (ImGui::Checkbox("Automatically Open Key Chests", &automaticKeyChests)) {
                    enhancementsettings::setAutomaticallyOpenKeyChests(automaticKeyChests);
                    saveEnhancements();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Automatically opens key chests spawned when a room is cleared.");
            }
            ImGui::TextDisabled("%s", enhancementNotice.empty() ? "Changes save automatically." : enhancementNotice.c_str());
            if (ImGui::Button("Back to game")) setVisible(false);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug")) {
            ImGui::BeginChild("debugNavigation", {105, 0}, ImGuiChildFlags_Borders);
            if (ImGui::Selectable("Player", debugPage == 0)) debugPage = 0;
            if (ImGui::Selectable("Checkpoints", debugPage == 1)) debugPage = 1;
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("debugContent", {0, 0});
            if (debugPage == 0) {
                ImGui::TextUnformatted("Player particles");
                ImGui::Checkbox("Hide trail dust", &debugParticles.hideTrailDust);
                ImGui::Checkbox("Hide breathe", &debugParticles.hideBreath);
                ImGui::Separator();
                ImGui::Checkbox("Red boxes around trail dust", &debugParticles.outlineTrailDust);
                ImGui::Checkbox("Red boxes around breathe", &debugParticles.outlineBreath);
                ImGui::TextDisabled("Visibility changes drawing only. Boxes still show when particles are hidden.");
            } else {
                ImGui::SeparatorText("Checkpoints");
                constexpr const char* destinations[]{"Mansion foyer", "Foyer after first key",
                    "Training room", "Gallery", "Foyer after training", "Parlor after candle cutscene"};
                for (unsigned i = 0; i < 6; ++i) {
                    if (ImGui::Button(destinations[i], {ImGui::GetContentRegionAvail().x, 32})) {
                        debugoverlay::requestCheckpoint(i + 1);
                        setVisible(false);
                    }
                }
            }
            if (ImGui::Button("Back to game")) setVisible(false);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::GetStyle() = savedStyle;
}
