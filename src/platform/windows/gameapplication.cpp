#include "resources/glme01addresses.h"
#include "platform/windows/gameapplication.h"
#include "platform/windows/debugoverlay.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <stdexcept>

namespace {
    gameapplication::Application* activeApplication = nullptr;

    LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        try {
            return activeApplication->handleMessage(window, message, wParam, lParam);
        } catch (const std::exception& exception) {
            activeApplication->close();
            MessageBoxA(window, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
            DestroyWindow(window);
            return 0;
        }
    }
}

void gameapplication::Application::prepareOpening(unsigned cutscene) {
    if (openingPrepared && preparedCutscene == cutscene) return;
    std::string error;
    gameassets::Package package;
    if (!opening.load(currentAssets, cutscene, error) || !package.open(currentAssets, error) ||
        !jaudio::readStream(package, cutscene == 0 ? "files/AudioRes/Stream/TMansion.afc" : "files/AudioRes/Stream/TMOpen.afc", openingAudio, error)) throw std::runtime_error(error);
    opening.prepareGraphics();
    openingPrepared = true;
    preparedCutscene = cutscene;
}

void gameapplication::Application::enterOpening(unsigned destination, unsigned cutscene) {
    prepareOpening(cutscene);
    openingDestination = destination;
    opening.reset();
    opening.update();
    openingUpdates = 1;
    openingActive = true;
    openingSkip = false;
    openingFade.reset();
    boundaryReported = false;
    if (audioEnabled) startupaudio::playStream(openingAudio);
}

void gameapplication::Application::advanceOpening() {
    if (openingFade.active()) {
        if (!openingFade.finished()) {
            openingFade.update();
            return;
        }
        openingActive = false;
        openingSkip = false;
        openingFade.reset();
        startPressed = false;
        if (openingDestination == 2) {
            gameplayScene.reset();
            gameplayScene = std::make_unique<gameplay::Scene>(currentAssets, gameEntry);
            gameplayScene->prepareGraphics();
            gameplayPhase = 0;
            gameplayActive = true;
            if (audioEnabled) startupaudio::stop();
            boundaryReported = false;
            return;
        }
        if (!gamerenderer::useScene("title")) gamerenderer::setInterfaceTextures(resources.textures);
        scene.enterTitle();
        boundaryReported = false;
        if (audioEnabled) {
            scene.takeSoundRequest();
            startupaudio::playTitle();
        }
        return;
    }
    const unsigned target = audioEnabled ? startupaudio::streamFrame() + 1 : openingUpdates + 1;
    while (openingUpdates < target && !opening.finished()) {
        opening.update();
        ++openingUpdates;
    }
    if (opening.finished() || openingSkip) {
        opening.draw(false);
        openingFade.begin();
        openingSkip = false;
        startPressed = false;
    }
}

void gameapplication::Application::enterMenu() {
    std::string error;
    if (!menuPrepared && !menu.load(currentAssets, error)) throw std::runtime_error(error);
    menuPrepared = true;
    menu.open();
    menu.reset();
    menuState = {};
    menuState.options = {gamesave::soundMode(saves), gamesave::rumbleEnabled(saves) ? 1U : 0U, saves.working[0xb1e]};
    menuInput = {};
    menuActive = true;
    boundaryReported = false;
    cardStatus = audioEnabled && card.status() == gamecard::LoadResult::unavailable ? 3 : 0;
    menu.update(menuState, saves);
    if (audioEnabled) startupaudio::playMenu();
}

void gameapplication::Application::advanceMenu() {
    const std::array<bool, 3> used{gamesave::slotUsed(saves, 0), gamesave::slotUsed(saves, 1), gamesave::slotUsed(saves, 2)};
    menuState.update(menuInput, used, cardStatus, card.status() == gamecard::LoadResult::damaged && !damageAcknowledged,
        menu.ready(menuState.selection));
    menuInput = {};
    for (const auto& request : menuState.requests) {
        if (request.action == fileselect::Action::selectFile && menuState.requestedScene == 4) {
            std::string error;
            if (!gameentry::selectNewFile(saves, request.source, gameEntry, error)) throw std::runtime_error(error);
        } else if (request.action == fileselect::Action::applyOptions) {
            for (unsigned i = 0; i < 3; ++i) saves.working[0xb1c + i] = static_cast<std::uint8_t>(menuState.options[i]);
        } else if (request.action == fileselect::Action::acknowledgeDamage) {
            damageAcknowledged = true;
            cardStatus = 3;
        } else if (request.action == fileselect::Action::copy || request.action == fileselect::Action::erase) {
            const auto now = std::time(nullptr);
            std::tm calendar{};
            localtime_s(&calendar, &now);
            std::snprintf(reinterpret_cast<char*>(cardBanner.data() + 0x1c20), 32, "Last saved on %d/%d", calendar.tm_mon + 1, calendar.tm_mday);
            std::string error;
            cardStatus = fileselect::writeCardAction(request, saves, defaults, card, cardBanner,
                static_cast<unsigned>(GetTickCount64()), static_cast<unsigned>(now - 946684800), error) ? 0 : 3;
            if (!error.empty()) OutputDebugStringA((error + "\n").c_str());
        } else if (request.action == fileselect::Action::format) {
            cardStatus = 3;
        }
    }
    for (const auto sound : menuState.sounds) if (audioEnabled) startupaudio::playMenuEffect(sound);
    menu.update(menuState, saves);
    if (menuState.requestedScene == 4 && gameEntry.initialized) {
        menuActive = false;
        enterOpening(2, gameEntry.cutscene);
        return;
    }
    if (menuState.requestedScene == 1) {
        menuActive = false;
        if (!gamerenderer::useScene("title")) gamerenderer::setInterfaceTextures(resources.textures);
        scene.enterTitle();
        boundaryReported = false;
    }
}

void gameapplication::Application::advance(bool audioFinished) {
    if (gameplayActive) {
        gameplayPhase ^= 1;
        if (!gameplayPhase) {
            const auto input = playerinput::pollWindowPad(applicationWindow, true);
            gameplayScene->update(input.status, input.port);
        }
        return;
    }
    if (openingActive) { advanceOpening(); return; }
    if (menuActive) { advanceMenu(); return; }
    scene.update(audioFinished, startPressed, saves.initialized);
    startPressed = false;
    if (scene.requestedScene() == 4) { enterOpening(); return; }
    const auto sound = scene.takeSoundRequest();
    if (sound == 0) return;
    if (jaudio::soundInfo(resources.audio, sound) == nullptr) throw std::runtime_error("Missing original sound metadata.");
    if (sound == 0x80000021) {
        if (audioEnabled) startupaudio::playTitle();
        return;
    }
    if (sound == 0x3802) {
        if (audioEnabled) startupaudio::playStartSound();
        if (scene.requestedScene() == 3) enterMenu();
        return;
    }
    const auto text = "JAudio effect playback pending: " + std::to_string(sound) + "\n";
    OutputDebugStringA(text.c_str());
}

void gameapplication::Application::draw(bool present) {
    if (debugoverlay::isVisible()) {
        std::string information = gameplayActive ? gameplayScene->debugInfo() : openingActive ? "Scene: Opening | Stream frame: " +
            std::to_string(openingUpdates) : menuActive ? "Scene: File selection / options" : "Scene: Boot / title";
        information += audioEnabled ? " | Audio: enabled" : " | Audio: disabled (capture)";
        debugoverlay::setScene(information + " | Cap: 30 FPS");
    }
    if (gameplayActive) { gameplayScene->draw(present); return; }
    if (openingActive) {
        if (openingFade.active()) {
            opening.drawFrozen(present, openingFade.alpha());
            return;
        }
        opening.draw(present);
        return;
    }
    if (menuActive) { menu.draw(present); return; }
    gamerenderer::drawInterface(scene.pictures(resources), present);
}

void gameapplication::Application::close() {
    gameplayScene.reset();
    startupaudio::close();
    gamerenderer::close();
}

void gameapplication::Application::restart() {
    gameplayScene.reset();
    gameplayActive = false;
    gameplayPhase = 0;
    gameEntry = {};
    openingFade.reset();
    openingActive = false;
    openingSkip = false;
    openingUpdates = 0;
    menuActive = false;
    menuState = {};
    menuInput = {};
    damageAcknowledged = false;
    cardStatus = 0;
    saves = defaults;
    if (!savePath.empty()) {
        std::string error;
        card.open(savePath, saves, error);
        if (!error.empty()) OutputDebugStringA((error + "\n").c_str());
    }
    if (!gamerenderer::useScene("title")) gamerenderer::setInterfaceTextures(resources.textures);
    scene.reset();
    startPressed = false;
    boundaryReported = false;
    draw(true);
    if (audioEnabled) startupaudio::playLogo(resources.logoSamples);
}

LRESULT gameapplication::Application::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (debugoverlay::handleMessage(window, message, wParam, lParam)) return 0;
    if (message == WM_KEYDOWN && (wParam == VK_F1 || wParam == VK_F5)) {
        if (!(lParam & (1LL << 30))) restart();
        return 0;
    }

    if (message == WM_SIZE) {
        gamerenderer::resize(std::max(1, static_cast<int>(LOWORD(lParam))), std::max(1, static_cast<int>(HIWORD(lParam))));
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_KEYDOWN) {
        const bool repeated = (lParam & (1LL << 30)) != 0;
        if (openingActive) {
            if (!repeated && wParam == 'A') openingSkip = true;
        } else if (menuActive) {
            unsigned button = 0;
            if (wParam == VK_LEFT) button = 1;
            if (wParam == VK_RIGHT) button = 2;
            if (wParam == VK_DOWN) button = 4;
            if (wParam == VK_UP) button = 8;
            if (wParam == 'A') button = 0x100;
            if (wParam == VK_RETURN) button = 0x1000;
            if (wParam == 'B') button = 0x200;
            if (wParam == 'X') button = 0x400;
            if (wParam == 'Y') button = 0x800;
            if (!repeated) menuInput.pressed |= button;
            menuInput.repeat |= button;
        } else if (!repeated && (wParam == VK_RETURN || wParam == 'A')) startPressed = true;
        if (wParam == VK_ESCAPE) DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        applicationWindow = nullptr;
        close();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}


void gameapplication::Application::initialize(HINSTANCE instance, const std::filesystem::path& assetPath, bool live, bool preload) {
    currentAssets = assetPath;
    std::string error;
    if (!startupresources::read(assetPath, resources, error)) throw std::runtime_error(error);
    if (!gamesave::initialize(resources.initialSaveFlags, saves, error)) throw std::runtime_error(error);
    defaults = saves;
    {
        gameassets::Package package;
        std::vector<std::uint8_t> executable, banner;
        if (!package.open(assetPath, error) || !package.read("sys/main.dol", executable, error) ||
            !dolresources::read(executable, glme01::memoryCardBanner, 0x1c00, banner, error)) throw std::runtime_error(error);
        std::copy(banner.begin(), banner.end(), cardBanner.begin());
        std::snprintf(reinterpret_cast<char*>(cardBanner.data() + 0x1c00), 32, "Luigi's Mansion");
    }
    if (live) {
        wchar_t executable[32768]{};
        if (!GetModuleFileNameW(nullptr, executable, 32768)) throw std::runtime_error("Cannot locate ghost-free-hum.");
        savePath = std::filesystem::path(executable).parent_path() / "saves" / "slot-a.gci";
        card.open(savePath, saves, error);
        if (!error.empty()) OutputDebugStringA((error + "\n").c_str());
    }
    audioEnabled = live;
    if (audioEnabled) startupaudio::prepare(assetPath, resources.audio);
    WNDCLASSW windowClass{};
    activeApplication = this;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"GhostFreeHumApplication";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&windowClass)) throw std::runtime_error("Could not register the game window.");
    window = CreateWindowW(windowClass.lpszClassName, L"ghost-free-hum", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 760, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) throw std::runtime_error("Could not create the game window.");
    applicationWindow = window;
    gamerenderer::open(window, resources.textures);
    gamerenderer::cacheScene("title");
    if (preload) {
        if (!menu.load(currentAssets, error)) throw std::runtime_error(error);
        menu.open();
        menuPrepared = true;
        prepareOpening();
        gamerenderer::useScene("title");
    }
}

int gameapplication::Application::run(int show) {
    ShowWindow(window, show);
    restart();
    using Clock = std::chrono::steady_clock;
    auto nextFrame = Clock::now();
    const auto clockStart = nextFrame;
    gameframepacing::Clock presentation;
    const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / gameframepacing::hostTicksPerSecond));
    bool running = true;
    while (running) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running) break;
        startupaudio::update();
        const auto now = Clock::now();
        if (now >= nextFrame) {
            while (nextFrame <= now) {
                advance(startupaudio::finished());
                nextFrame += period;
            }
            if (presentation.ready(std::chrono::duration<double>(now - clockStart).count())) draw(true);
            if (menuActive && menuState.requestedScene != -1 && !boundaryReported) {
                boundaryReported = true;
                MessageBoxW(window, L"This destination scene is not ported yet. Press F1 to restart.", L"ghost-free-hum", MB_OK);
                nextFrame = Clock::now() + period;
            } else if (!menuActive && !openingActive && !gameplayActive && scene.requestedScene() != -1 && !boundaryReported) {
                boundaryReported = true;
                if (scene.requestedScene() == 3) {
                    MessageBoxW(window, L"The original file-selection scene is not ported yet.", L"ghost-free-hum", MB_OK);
                    nextFrame = Clock::now() + period;
                }
            }
        } else {
            MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }
    return 0;
}
