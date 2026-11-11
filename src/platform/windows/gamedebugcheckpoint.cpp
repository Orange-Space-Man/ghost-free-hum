#include "platform/windows/gameapplication.h"
#include "platform/windows/debugoverlay.h"
#include "game/audio/jaudiopositionalplayback.h"

#include <cmath>
#include <stdexcept>

void gameapplication::Application::loadDebugCheckpoint(unsigned checkpoint) {
    if (checkpoint < 1 || checkpoint > 2) return;
    debugLoading = true;
    debugCloseRequested = false;
    struct LoadingScope {
        Application& application;
        ~LoadingScope() {
            debugoverlay::setLoading({});
            application.debugLoading = false;
            application.resetHostClock = true;
        }
    } loading{*this};
    using Clock = std::chrono::steady_clock;
    auto nextProgress = Clock::now();
    std::string log = "F3 + " + std::to_string(checkpoint) + " - Preparing checkpoint\n";
    std::string stage;
    unsigned frame = 0;
    const auto progress = [&](const char* message = nullptr) {
        const auto now = Clock::now();
        if (!message && now < nextProgress) return;
        if (message) {
            if (!stage.empty()) log += stage + " - done\n";
            stage = message;
        }
        MSG pending{};
        while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) {
            if (pending.message == WM_QUIT) { debugCloseRequested = true; continue; }
            TranslateMessage(&pending);
            DispatchMessageW(&pending);
        }
        debugoverlay::setLoading(log + stage + "...\n" +
            (debugCloseRequested ? "Closing after preparation finishes..." : "Simulated frames: " + std::to_string(frame)));
        gamerenderer::drawInterface({}, true);
        nextProgress = now + std::chrono::milliseconds(33);
    };
    progress("Finishing previous scene preparation");
    if (gameplayPreparation.valid()) {
        while (gameplayPreparation.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) progress();
    }
    gameplayPreparation = {};
    startupaudio::bindPositionalPlayback({});
    startupaudio::stop();
    gameplayScene.reset();
    gameplayActive = openingActive = menuActive = false;
    openingFade.reset();
    openingSkip = startPressed = false;
    menuInput = {};
    gameplayPhase = 0;
    auto scratch = defaults;
    std::string error;
    if (!gameentry::selectNewFile(scratch, 0, gameEntry, error)) throw std::runtime_error(error);
    progress("Loading room, actors and audio resources");
    auto preparation = std::async(std::launch::async, [this] {
        return std::make_unique<gameplay::Scene>(currentAssets, gameEntry);
    });
    while (preparation.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) progress();
    gameplayScene = preparation.get();
    progress("Preparing models and shaders");
    auto graphics = std::async(std::launch::async, [this] {
        gameplayScene->prepareModelGraphics(gamerenderer::modelDevice());
    });
    while (graphics.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) progress();
    graphics.get();
    progress("Preparing scene graphics");
    gameplayScene->prepareGraphics();
    auto playback = gameplayScene->playback();
    jaudio::DspMixer mixer;
    unsigned audioRemainder = 0;
    const auto step = [&](const playerinput::PadStatus& pad) {
        progress();
        gameplayScene->update(pad, 0, frame++ * 2);
        gameplayScene->updatePresentation(true);
        gameplayScene->updateAudio();
        gameplayScene->updatePresentation(false);
        gameplayScene->updateAudio();
        audioRemainder += 32000;
        while (audioRemainder >= 2400) {
            std::array<std::int16_t, 160> discarded{};
            mixer.begin();
            if (!playback->mix(mixer, error)) throw std::runtime_error(error);
            mixer.end(discarded, 1);
            audioRemainder -= 2400;
        }
    };
    const auto until = [&](const auto& ready, unsigned limit, const char* failure) {
        for (unsigned i = 0; i < limit && !ready(); ++i) step({});
        if (!ready()) throw std::runtime_error(failure);
    };
    progress("Advancing foyer entrance cutscene");
    until([&] { return gameEntry.inventory[0x20 + 48] == 1; }, 600, "Debug checkpoint: entrance event did not finish.");
    if (checkpoint == 2) {
        const auto walk = [&](float targetX, float targetZ) {
            for (unsigned i = 0; i < 500; ++i) {
                const auto& position = *gameplayScene->playerView().position;
                const float x = targetX - position[0], z = targetZ - position[2];
                const float distance = std::sqrt(x * x + z * z);
                if (distance < 20) return;
                playerinput::PadStatus pad;
                pad.stickX = static_cast<std::int8_t>(87 * x / distance);
                pad.stickY = static_cast<std::int8_t>(-87 * z / distance);
                step(pad);
            }
            throw std::runtime_error("Debug checkpoint: foyer route did not reach its waypoint.");
        };
        progress("Walking upstairs to the locked door");
        walk(800, 100);
        walk(800, -600);
        walk(0, -600);
        walk(0, -950);
        walk(100, -1150);
        playerinput::PadStatus pad;
        pad.stickY = 87;
        for (unsigned i = 0; i < 6; ++i) step(pad);
        step({});
        pad = {};
        pad.buttons = 0x100;
        step(pad);
        progress("Advancing locked-door interaction");
        bool presented = false;
        until([&] {
            presented |= gameplayScene->keyPresentationActive();
            return presented && !gameplayScene->keyPresentationActive();
        }, 400, "Debug checkpoint: locked-door presentation did not finish.");
        for (unsigned i = 0; i < 120; ++i) step({});
        progress("Returning downstairs");
        walk(0, -950);
        walk(0, -600);
        walk(-800, -600);
        walk(-800, 100);
        pad = {};
        pad.stickX = 87;
        for (unsigned i = 0; i < 45; ++i) step(pad);
        progress("Advancing ghost and key cutscene");
        until([&] { return gameEntry.inventory[0x20 + 7] == 1; }, 850, "Debug checkpoint: ghost/key event did not finish.");
    }
    progress("Finishing checkpoint");
    step({});
    debugoverlay::setLoading({});
    if (debugCloseRequested) {
        PostMessageW(window, WM_CLOSE, 0, 0);
        return;
    }
    gameplayActive = true;
    boundaryReported = false;
    if (audioEnabled) startupaudio::bindPositionalPlayback(playback);
    draw(true);
}
