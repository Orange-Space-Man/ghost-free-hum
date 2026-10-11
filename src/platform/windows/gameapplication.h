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
#include "game/scene/gameentrystate.h"
#include "platform/windows/windowpadinput.h"
#include <windows.h>

namespace gameapplication {
    class Application {
    public:
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
        LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    protected:
        startupresources::Resources resources;
        startupscene::Scene scene;
        gamesave::State saves;
        gamecard::Storage card;
        bool audioEnabled = false;
        bool startPressed = false;
        bool boundaryReported = false;
        std::filesystem::path currentAssets;
        std::filesystem::path savePath;
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
        unsigned openingDestination = 1;
        unsigned preparedCutscene = 0x4c;

        HWND window = nullptr;
    };
}
