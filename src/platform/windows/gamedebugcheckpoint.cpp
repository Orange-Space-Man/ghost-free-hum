#include "platform/windows/gameapplication.h"
#include "platform/windows/debugoverlay.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/enemies/enemyghostparent.h"
#include "game/actors/characterowner.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>

void gameapplication::Application::loadDebugCheckpoint(unsigned checkpoint) {
    if (checkpoint < 1 || checkpoint > 5) return;
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
    std::string log = "Preparing checkpoint " + std::to_string(checkpoint) + "\n";
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
    transitionPhase = TransitionPhase::Idle;
    transitionWipe = {};
    transitionFrame = {};
    transitionDestination = -1;
    startupaudio::bindPositionalPlayback({});
    startupaudio::stop();
    gameplayScene.reset();
    gameplayActive = openingActive = menuActive = false;
    openingFade.reset();
    openingSkip = startPressed = false;
    menuInput = {};
    gameplayPhase = 0;
    checkpointSaves = defaults;
    gameplaySaves = &checkpointSaves;
    std::string error;
    if (!gameentry::selectNewFile(checkpointSaves, 0, gameEntry, error)) throw std::runtime_error(error);
    const bool cached = checkpointStates[checkpoint - 1].has_value();
    if (cached) {
        checkpointSaves = checkpointStates[checkpoint - 1]->saves;
        gameEntry = checkpointStates[checkpoint - 1]->entry;
    }
    progress("Loading room, actors and audio resources");
    auto preparation = std::async(std::launch::async, [this] {
        return std::make_unique<gameplay::Scene>(currentAssets, gameEntry);
    });
    while (preparation.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) progress();
    gameplayScene = preparation.get();
    gameplayScene->bindControllerOptions(*gameplaySaves, &presentationFrames);
    bindGameplaySave();
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
    if (!cached) {
        progress("Advancing foyer entrance cutscene");
        until([&] { return gameEntry.inventory[0x20 + 48] == 1; }, 600, "Debug checkpoint: entrance event did not finish.");
        if (checkpoint >= 2) {
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
            if (checkpoint >= 3) {
                progress("Collecting the Parlor key");
                walk(-2.0035901f, -61.82265f);
                for (unsigned i = 0; i < 180; ++i) step({});
                progress("Entering Parlor");
                walk(800, 100); walk(800, -600); walk(0, -600); walk(0, -950); walk(100, -1150);
                pad = {}; pad.stickY = 87;
                for (unsigned i = 0; i < 6; ++i) step(pad);
                step({}); pad = {}; pad.buttons = 0x100; step(pad);
                const auto dialogue = [&](unsigned destination, unsigned limit) {
                    for (unsigned i = 0; i < limit && gameplayScene->transition().destination < 0; ++i) {
                        playerinput::PadStatus input;
                        if (i % 30 == 0) input.buttons = 0x100;
                        step(input);
                    }
                    if (gameplayScene->transition().destination != static_cast<int>(destination))
                        throw std::runtime_error("Debug checkpoint: dialogue did not reach its destination.");
                };
                const auto handoff = [&] {
                    beginTransition();
                    for (unsigned vi = 0; transitionPhase != TransitionPhase::Idle; ++vi) {
                        if (vi > 10000) throw std::runtime_error("Debug checkpoint: scene handoff did not finish.");
                        progress();
                        advanceTransition();
                        if (transitionPhase == TransitionPhase::Loading) Sleep(1);
                    }
                    playback = gameplayScene->playback();
                    mixer.reset();
                    audioRemainder = 0;
                };
                progress("Advancing Parlor rescue");
                dialogue(1, 4000);
                handoff();
                progress("Advancing E. Gadd's introduction");
                dialogue(3, 3000);
                handoff();
                if (checkpoint >= 4) {
                    progress("Completing the original training event");
                    bool waiting = false;
                    unsigned ghostRounds = 0;
                    bool selectedLab = false;
                    for (unsigned i = 0; i < 10000 && gameplayScene->transition().destination < 0; ++i) {
                        auto& managers = gameplayScene->checkpointResources().managers();
                        auto* player = managers.playerEvents().player(0);
                        auto& entries = *managers.players().entries;
                        actors::MoveActor* ghost = nullptr;
                        for (unsigned index = 0; index < managers.actors().count() && !ghost; ++index) {
                            auto* actor = managers.actors().actor(index);
                            if (actor && actor->type() == 76 && actor->position[0] > -31000 && !(actor->flags & 0xc000)) ghost = actor;
                        }
                        playerinput::PadStatus input;
                        if (i % 6 < 2) input.buttons = 0x100;
                        if (checkpoint == 5 && !selectedLab &&
                            gameplayScene->checkpointResources().messages().choices().lines.size() == 2) {
                            input.buttons = 4;
                            selectedLab = true;
                        }
                        if (ghost && waiting && !(player->flags & 0x10)) {
                            input.triggerRight = 255;
                            const auto& weapon = *player->components->weapon->first;
                            if (!entries.entries[0]->state) {
                                ghost->setPosition({weapon.position[0] + 60 * weapon.field610[0],
                                    weapon.position[1] + 60 * weapon.field610[1],
                                    weapon.position[2] + 60 * weapon.field610[2]});
                                ghost->setVelocity({0, 0, 0});
                            } else {
                                const float dx = player->position[0] - ghost->position[0];
                                const float dz = player->position[2] - ghost->position[2];
                                const float length = std::max(0.001f, std::hypot(dx, dz));
                                input.stickX = static_cast<std::int8_t>(87 * dx / length);
                                input.stickY = static_cast<std::int8_t>(-87 * dz / length);
                            }
                            if (auto* owner = dynamic_cast<characterowner::Owner*>(managers.enemies().activeOwner(*ghost))) {
                                auto* parent = dynamic_cast<ghostparent::Parent*>(owner->ownership().strategy.get());
                                if (parent && !parent->lightState) { characterevent::Event flash(2); owner->event(flash); }
                            }
                        }
                        step(input);
                        if (managers.flags().value(87)) waiting = true;
                        else if (waiting) { ++ghostRounds; waiting = false; }
                    }
                    if (gameplayScene->transition().destination != (checkpoint == 4 ? 6 : 1) || ghostRounds < 4 ||
                        (checkpoint == 5 && !selectedLab))
                        throw std::runtime_error("Debug checkpoint: training did not finish with the selected original choice.");
                    progress(checkpoint == 4 ? "Entering Gallery after training" : "Returning to Lab after training");
                    handoff();
                    if (checkpoint == 5) {
                        progress("Choosing Mansion in the Lab");
                        for (unsigned i = 0; i < 3000 &&
                            gameplayScene->checkpointResources().messages().choices().lines.empty(); ++i) {
                            pad = {}; if (i % 20 == 0) pad.buttons = 0x100;
                            step(pad);
                        }
                        if (gameplayScene->checkpointResources().messages().choices().lines.empty())
                            throw std::runtime_error("Debug checkpoint: Lab choice did not appear.");
                        if (gameplayScene->checkpointResources().messages().choices().lines.front().find("Mansion") == std::string::npos)
                            throw std::runtime_error("Debug checkpoint: Lab first choice is not Mansion.");
                        for (unsigned i = 0; i < 60; ++i) step({});
                        pad = {}; pad.buttons = 0x100; step(pad); step({});
                        dialogue(2, 3000);
                        progress("Advancing the exterior movie");
                        beginTransition();
                        if (!openingActive || preparedCutscene != 0)
                            throw std::runtime_error("Debug checkpoint: Lab did not start the post-training exterior movie.");
                        openingSkip = true;
                        for (unsigned i = 0; i < 10000 && openingActive; ++i) {
                            progress();
                            advanceOpening();
                            if (!gameplayScene) Sleep(1);
                        }
                        if (openingActive || !gameplayScene || gameEntry.map != 2)
                            throw std::runtime_error("Debug checkpoint: exterior movie did not enter the Foyer.");
                        playback = gameplayScene->playback();
                        mixer.reset();
                        audioRemainder = 0;
                        progress("Advancing the post-training Foyer arrival");
                        for (unsigned i = 0; i < 60; ++i) step({});
                        for (unsigned i = 0; i < 3000 && gameplayScene->checkpointResources().entranceActive(); ++i) {
                            pad = {}; if (i % 20 == 0) pad.buttons = 0x100;
                            step(pad);
                        }
                        if (gameplayScene->checkpointResources().entranceActive())
                            throw std::runtime_error("Debug checkpoint: post-training Foyer did not release control.");
                    }
                }
            }

        }
    }
    progress("Finishing checkpoint");
    step({});
    if (!cached) checkpointStates[checkpoint - 1] = DebugCheckpointState{checkpointSaves, gameEntry};
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
