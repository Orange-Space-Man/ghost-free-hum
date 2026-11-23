#pragma once

#include "resources/dolresources.h"
#include "game/scene/gameplayscene.h"
#include "platform/windows/gameassetsetup.h"
#include "platform/d3d11/gamerenderer.h"
#include "platform/windows/startupaudio.h"
#include "game/save/gamesavestate.h"
#include "game/save/gamecardstorage.h"
#include "game/menu/menulayoutgraphics.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "game/menu/fileselectpanes.h"
#include "game/menu/fileselectscene.h"
#include "game/menu/fileselectcard.h"
#include "game/startup/openingscene.h"
#include "platform/windows/gameframepacing.h"
#include "game/startup/scenetransition.h"
#include "platform/d3d11/transitionframecapture.h"
#include "game/scene/gameentrystate.h"
#include "platform/windows/windowpadinput.h"
#include <windows.h>
#include <future>
#include <optional>

namespace gameapplication {
    class Application {
    public:
        HWND initializeSetup(HINSTANCE instance, int show);
        void initialize(HINSTANCE instance, const std::filesystem::path& assetPath, bool live, bool preload);
        int run(int show);
        void prepareOpening(unsigned cutscene = 0x4c);
        void enterOpening(unsigned destination = 1, unsigned cutscene = 0x4c);
        void advanceOpening();
        void enterMenu();
        void advanceMenu();
        void advance(bool audioFinished);
        void draw(bool present);
        void close();
        void restart();
        void loadDebugCheckpoint(unsigned checkpoint);
        void bindGameplaySave();
        void saveGameplay();
        LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    protected:
        void advanceScene(bool audioFinished);
        void advanceGameplay();
        void beginTransition();
        void advanceTransition();
        enum class TransitionPhase { Idle, Outgoing, Loading, Incoming };
        TransitionPhase transitionPhase = TransitionPhase::Idle;
        int transitionDestination = -1;
        scenetransition::ParlorWipe transitionWipe;
        scenetransition::FrameCapture transitionFrame;
        startupresources::Resources resources;
        startupscene::Scene scene;
        gamesave::State saves;
        gamesave::State checkpointSaves;
        struct DebugCheckpointState {
            gamesave::State saves;
            gameentry::State entry;
        };
        std::array<std::optional<DebugCheckpointState>, 6> checkpointStates;
        std::uint64_t presentationFrames = 0;
        gamesave::State* gameplaySaves = &saves;
        gamecard::Storage card;
        bool audioEnabled = false;
        bool startPressed = false;
        unsigned previousMenuButtons = 0, menuHoldTicks = 0;
        bool boundaryReported = false;
        std::filesystem::path currentAssets;
        std::filesystem::path savePath;
        std::string saveGameId = "GLME01";
        fileselect::Scene menu;
        fileselect::State menuState;
        fileselect::Input menuInput;
        gamesave::State defaults;
        gamecard::Block cardBanner{};
        bool menuActive = false;
        bool damageAcknowledged = false;
        unsigned cardStatus = 0;
        openingscene::Scene opening;
        bool openingActive = false;
        bool openingSkip = false;
        scenetransition::OutgoingFade openingFade;
        unsigned openingUpdates = 0;
        bool openingPrepared = false, menuPrepared = false;
        jaudio::StreamAudio openingAudio;
        gameentry::State gameEntry;
        bool gameplayActive = false;
        unsigned gameplayPhase = 0;
        HWND applicationWindow = nullptr;
        std::unique_ptr<gameplay::Scene> gameplayScene;
        std::future<std::unique_ptr<gameplay::Scene>> gameplayPreparation;
        unsigned openingDestination = 1;
        unsigned preparedCutscene = 0x4c;

        bool debugLoading = false, debugCloseRequested = false, resetHostClock = false;
        unsigned debugCheckpoint = 0;
        HWND window = nullptr;
    };
}
