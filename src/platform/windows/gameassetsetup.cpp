#include "platform/windows/gameassetsetup.h"
#include "platform/d3d11/gamerenderer.h"
#include "resources/gameassetpackage.h"
#include "resources/roomdiscreader.h"

#include "imgui.h"
#include <commdlg.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
    struct Installed {
        std::string id;
        std::filesystem::path path;
    };

    enum class Page { none, importing, result, select };
    Page page = Page::none;
    std::vector<Installed> installed;
    std::vector<std::string> lines;
    std::mutex lineMutex;
    std::atomic<unsigned> percent{0};
    bool succeeded = false, continueRequested = false, scrollToEnd = false, alwaysSelect = false;
    unsigned selected = 0;
    std::filesystem::path chosen;
    HWND host = nullptr;

    std::filesystem::path executablePath() {
        wchar_t executable[32768]{};
        if (!GetModuleFileNameW(nullptr, executable, 32768)) throw std::runtime_error("Cannot locate ghost-free-hum.");
        return executable;
    }

    std::filesystem::path assetDirectory() {
        wchar_t configured[32768]{};
        const auto length = GetEnvironmentVariableW(L"GHOST_FREE_HUM_ASSETS", configured, 32768);
        if (length > 0 && length < 32768) return configured;
        return executablePath().parent_path() / "assets";
    }

    std::filesystem::path preferencePath() {
        wchar_t path[32768]{};
        const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, 32768);
        if (length == 0 || length >= 32768) return {};
        return std::filesystem::path(path) / L"ghost-free-hum" / L"region.txt";
    }

    std::string preferred() {
        std::ifstream input(preferencePath());
        std::string value;
        if (input) std::getline(input, value);
        return value;
    }

    void savePreferred(const std::string& id) {
        const auto path = preferencePath();
        if (path.empty()) throw std::runtime_error("Cannot locate the region preference folder.");
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::trunc);
        if (!output || !(output << id << '\n')) throw std::runtime_error("Cannot save the preferred edition.");
    }

    const char* label(const std::string& id) {
        if (id == "GLME01") return "North America (GLME01)";
        if (id == "GLMJ01") return "Japan (GLMJ01)";
        if (id == "GLMP01") return "Europe / PAL (GLMP01)";
        return "Unknown edition";
    }

    std::vector<Installed> findInstalled() {
        std::vector<Installed> result;
        for (const char* id : {"GLME01", "GLMJ01", "GLMP01"}) {
            std::string lower = id;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const auto path = assetDirectory() / (lower + ".ppak");
            if (!std::filesystem::exists(path)) continue;
            gameassets::Package package;
            std::string error;
            if (!package.open(path, error) || package.gameId() != id)
                throw std::runtime_error("Invalid " + std::string(id) + " asset package: " + (error.empty() ? "disc identity mismatch" : error));
            result.push_back({id, path});
        }
        return result;
    }

    void append(const std::string& line) {
        std::lock_guard lock(lineMutex);
        lines.push_back(line);
        scrollToEnd = true;
    }

    void pump() {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { continueRequested = true; continue; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (IsWindow(host)) gamerenderer::drawInterface({}, true);
        MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }

    std::filesystem::path importOne() {
        wchar_t filename[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = host;
        dialog.lpstrFilter = L"GameCube ISO (*.iso)\0*.iso\0\0";
        dialog.lpstrFile = filename;
        dialog.nMaxFile = 32768;
        dialog.lpstrTitle = L"Import Luigi's Mansion (North America, Japan, or Europe)";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&dialog)) return {};

        lines.clear();
        percent = 0;
        succeeded = false;
        continueRequested = false;
        page = Page::importing;
        append("Checking selected disc");
        std::string id, error;
        std::filesystem::path destination;
        if (!roomdisc::readGameId(filename, id, error)) {
            append(error);
        } else {
            append(std::string("Disc: ") + label(id));
            std::string lower = id;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            destination = assetDirectory() / (lower + ".ppak");
            if (std::filesystem::exists(destination)) {
                append("This edition is already imported.");
                succeeded = true;
            } else {
                auto job = std::async(std::launch::async, [&, destination] {
                    return gameassets::importDisc(filename, destination, error,
                        [&](std::uint64_t done, std::uint64_t total) {
                            percent = total ? static_cast<unsigned>(done * 100 / total) : 100;
                            return IsWindow(host) != FALSE;
                        }, [&](const std::string& stage) { append(stage); });
                });
                while (job.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) pump();
                succeeded = job.get();
                if (!succeeded) append(error);
                else append("Assets saved to " + destination.filename().string());
            }
        }
        page = Page::result;
        while (!continueRequested && IsWindow(host)) pump();
        page = Page::none;
        return succeeded ? destination : std::filesystem::path{};
    }
}

bool gameassetsetup::active() { return page != Page::none; }

bool gameassetsetup::handleMessage(UINT message, WPARAM key) {
    if (page == Page::result && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_LBUTTONDOWN)) {
        continueRequested = true;
        return true;
    }
    if (page == Page::select && message == WM_KEYDOWN && !installed.empty()) {
        const auto count = static_cast<unsigned>(installed.size());
        if (key == VK_UP) selected = (selected + count - 1) % count;
        if (key == VK_DOWN) selected = (selected + 1) % count;
        if (key == VK_RETURN) chosen = installed[selected].path;
    }
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN ||
        message == WM_SYSKEYUP || message == WM_CHAR || message == WM_LBUTTONDOWN ||
        message == WM_LBUTTONUP || message == WM_MOUSEWHEEL;
}

void gameassetsetup::draw() {
    if (page == Page::none) return;
    const auto display = ImGui::GetIO().DisplaySize;
    if (page == Page::select) {
        const auto savedStyle = ImGui::GetStyle();
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        const ImVec4 green{0.51f, 0.85f, 0.35f, 1};
        style.Colors[ImGuiCol_WindowBg] = {0.075f, 0.09f, 0.08f, 1};
        style.Colors[ImGuiCol_CheckMark] = green;
        for (auto color : {ImGuiCol_Button, ImGuiCol_Header, ImGuiCol_FrameBg})
            style.Colors[color] = {0.16f, 0.28f, 0.19f, 1};
        for (auto color : {ImGuiCol_ButtonHovered, ImGuiCol_HeaderHovered, ImGuiCol_FrameBgHovered})
            style.Colors[color] = {0.24f, 0.43f, 0.28f, 1};
        for (auto color : {ImGuiCol_ButtonActive, ImGuiCol_HeaderActive, ImGuiCol_FrameBgActive})
            style.Colors[color] = {0.21f, 0.38f, 0.24f, 1};
        ImGui::PushFont(nullptr, 20);
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(display);
        ImGui::Begin("Edition setup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
        const float width = std::min(560.0f, std::max(300.0f, display.x - 64.0f));
        const float left = std::max(24.0f, (display.x - width) * 0.5f);
        const float top = std::max(44.0f, (display.y - 440.0f) * 0.5f);
        ImGui::SetCursorPos({left, top});
        ImGui::TextColored(green, "Select game edition");
        ImGui::SetCursorPosX(left);
        ImGui::TextUnformatted("Choose the original edition to play:");
        ImGui::Dummy({0, 16});
        for (unsigned i = 0; i < installed.size(); ++i) {
            ImGui::SetCursorPosX(left);
            if (ImGui::RadioButton(label(installed[i].id), selected == i)) selected = i;
            ImGui::Dummy({0, 14});
        }
        ImGui::Dummy({0, 12});
        ImGui::SetCursorPosX(left);
        ImGui::Checkbox("Always select this option", &alwaysSelect);
        ImGui::SetCursorPosX(left);
        ImGui::TextDisabled("Hold Shift while launching to show this screen again.");
        ImGui::Dummy({0, 15});
        ImGui::SetCursorPosX(left);
        if (ImGui::Button("Play selected edition")) chosen = installed[selected].path;
        ImGui::End();
        ImGui::PopFont();
        ImGui::GetStyle() = savedStyle;
        return;
    }
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::Begin("Edition setup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted("Importing Luigi's Mansion assets");
    ImGui::Separator();
    {
        ImGui::BeginChild("Import steps", {0, -48}, 0, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard lock(lineMutex);
            for (const auto& line : lines) ImGui::TextUnformatted(line.c_str());
            if (page == Page::importing) ImGui::Text("Copying files: %u%%", percent.load());
            if (scrollToEnd) { ImGui::SetScrollHereY(1); scrollToEnd = false; }
        }
        ImGui::EndChild();
        if (page == Page::result) {
            ImGui::TextColored(succeeded ? ImVec4(0.4f, 1, 0.35f, 1) : ImVec4(1, 0.35f, 0.35f, 1),
                "%s - Press any key to continue...", succeeded ? "Import successful" : "Import unsuccessful");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

std::filesystem::path gameassetsetup::prepare(HWND window, bool forceSelect, bool forceImport) {
    host = window;
    forceSelect |= forceImport;
    installed = findInstalled();
    if (forceImport || installed.empty()) {
        const auto imported = importOne();
        installed = findInstalled();
        if (installed.empty()) return {};
        if (!imported.empty() && installed.size() == 1) return imported;
    }
    if (installed.size() == 1) return installed.front().path;
    if (!forceSelect) {
        const auto saved = preferred();
        for (const auto& edition : installed) if (edition.id == saved) return edition.path;
    }
    page = Page::select;
    chosen.clear();
    selected = 0;
    const auto saved = preferred();
    alwaysSelect = false;
    for (std::size_t i = 0; i < installed.size(); ++i) {
        if (installed[i].id == saved) { selected = static_cast<unsigned>(i); alwaysSelect = true; break; }
    }
    while (chosen.empty() && IsWindow(window)) pump();
    page = Page::none;
    if (alwaysSelect && !chosen.empty()) {
        for (const auto& edition : installed) if (edition.path == chosen) savePreferred(edition.id);
    } else if (!alwaysSelect) {
        const auto path = preferencePath();
        if (!path.empty()) { std::error_code ignored; std::filesystem::remove(path, ignored); }
    }
    return chosen;
}

bool gameassetsetup::multipleEditionsAvailable() {
    try { return findInstalled().size() > 1; }
    catch (...) { return false; }
}

bool gameassetsetup::rebootToRegionSelect(bool importAnother) {
    const auto executable = executablePath();
    std::wstring command = L"\"" + executable.wstring() + L"\" " + (importAnother ? L"--import" : L"--region-select");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
        executable.parent_path().c_str(), &startup, &process)) return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

