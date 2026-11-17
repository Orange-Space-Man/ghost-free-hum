#include "platform/windows/controlsettings.h"
#include "platform/windows/controlbindings.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    HWND host = nullptr;
    bool opened = false;
    unsigned selected = 0;
    int listening = -1;
    double listenUntil = 0;
    controlbindings::Snapshot previous;
    std::string notice;
    bool saveFailed = false;
    const ImVec4 green{0.51f, 0.85f, 0.35f, 1}, muted{0.56f, 0.65f, 0.59f, 1};

    void save() {
        std::string error;
        saveFailed = !controlbindings::persist(error);
        notice = saveFailed ? error : "Changes saved";
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
    host = window; opened = false; listening = -1;
    wchar_t path[32768]{};
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, 32768);
    if (length && length < 32768) controlbindings::initialize(std::filesystem::path(path) / L"ghost-free-hum" / L"controls.ini");
    notice = controlbindings::loadError();
    saveFailed = !notice.empty();

}

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
            ImGui::Selectable("Controls", true);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("content", {0, 0});
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
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::GetStyle() = savedStyle;
}
