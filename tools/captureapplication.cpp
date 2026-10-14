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

class CaptureApplication : public gameapplication::Application {
public:
    int run(HINSTANCE instance);
};

int CaptureApplication::run(HINSTANCE instance) {
    std::filesystem::path capturePath;
    try {
        std::filesystem::path assetPath;
        int captureFrame = -1;
        bool captureDoorMaterial = false;
        int captureMenu = -1;
        int optionsFrame = -1;
        int previewFrame = -1;
        int fileFrame = -1;
        int openingFrame = -1;
        unsigned cutsceneId = 0x4c;
        bool captureAudio = false;
        bool captureStats = false;
        bool captureMenuFlow = false;
        bool captureOpeningFlow = false;
        bool captureNewGameFlow = false;
        bool profileOpening = false;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv != nullptr) {
            for (int index = 1; index < argc; ++index) {
                const std::wstring value = argv[index];
                if (value == L"--capture-cutscene" && index + 3 < argc) {
                    capturePath = argv[++index];
                    cutsceneId = std::stoul(argv[++index]);
                    openingFrame = std::stoi(argv[++index]);
                    if (openingFrame < 0 || openingFrame > 3000) throw std::runtime_error("Invalid cutscene capture frame.");
                } else if (value == L"--capture-door-material" && index + 1 < argc) {
                    captureDoorMaterial = true;
                    capturePath = argv[++index];
                } else if (value == L"--debug-overlay") {
                    captureStats = true;
                } else if (value == L"--assets" && index + 1 < argc) {
                    assetPath = argv[++index];
                } else if (value == L"--capture-new-game-flow" && index + 1 < argc) {
                    capturePath = argv[++index];
                    captureNewGameFlow = true;
                } else if (value == L"--capture-opening-flow" && index + 1 < argc) {
                    capturePath = argv[++index];
                    captureOpeningFlow = true;
                } else if (value == L"--profile-opening") {
                    profileOpening = true;
                } else if (value == L"--capture-opening" && index + 2 < argc) {
                    capturePath = argv[++index];
                    openingFrame = std::stoi(argv[++index]);
                    if (openingFrame < 0 || openingFrame > 3000) throw std::runtime_error("Invalid opening capture frame.");
                } else if (value == L"--capture-menu-flow" && index + 1 < argc) {
                    capturePath = argv[++index];
                    captureMenuFlow = true;
                } else if (value == L"--capture-file-select" && index + 2 < argc) {
                    capturePath = argv[++index];
                    const std::wstring frame = argv[++index];
                    if (frame.empty() || frame.size() > 4 || frame.find_first_not_of(L"0123456789") != std::wstring::npos) {
                        LocalFree(argv);
                        throw std::runtime_error("Invalid file-selection capture frame.");
                    }
                    fileFrame = std::stoi(frame);
                } else if (value == L"--capture-menu-previews" && index + 2 < argc) {
                    capturePath = argv[++index];
                    const std::wstring frame = argv[++index];
                    if (frame.empty() || frame.size() > 5 || frame.find_first_not_of(L"0123456789") != std::wstring::npos) {
                        LocalFree(argv);
                        throw std::runtime_error("Invalid preview frame.");
                    }
                    previewFrame = std::stoi(frame);
                } else if (value == L"--capture-menu-options" && index + 2 < argc) {
                    capturePath = argv[++index];
                    const std::wstring frame = argv[++index];
                    if (frame.empty() || frame.size() > 5 || frame.find_first_not_of(L"0123456789") != std::wstring::npos) {
                        LocalFree(argv);
                        throw std::runtime_error("Invalid options capture frame.");
                    }
                    optionsFrame = std::stoi(frame);
                    captureMenu = 2;
                } else if (value == L"--capture-menu-layout" && index + 2 < argc) {
                    capturePath = argv[++index];
                    const std::wstring layout = argv[++index];
                    if (layout != L"0" && layout != L"1" && layout != L"2") {
                        LocalFree(argv);
                        throw std::runtime_error("Invalid menu layout index.");
                    }
                    captureMenu = std::stoi(layout);
                } else if (value == L"--capture-startup" && index + 2 < argc) {
                    capturePath = argv[++index];
                    const std::wstring frame = argv[++index];
                    if (frame.empty() || frame.find_first_not_of(L"0123456789") != std::wstring::npos || frame.size() > 5) {
                        LocalFree(argv);
                        throw std::runtime_error("Invalid startup frame.");
                    }
                    captureFrame = std::stoi(frame);
                } else if (value == L"--capture-audio") {
                    captureAudio = true;
                } else {
                    LocalFree(argv);
                    throw std::runtime_error("Unknown command line option.");
                }
            }
            LocalFree(argv);
        }
        if (captureFrame > 10000 || (captureAudio && capturePath.empty())) throw std::runtime_error("Invalid capture options.");
        if (assetPath.empty()) assetPath = gameassetsetup::prepare();
        if (assetPath.empty()) return 0;
        initialize(instance, assetPath, false, captureOpeningFlow || captureMenuFlow);
        if (captureStats) {
            SendMessageW(window, WM_KEYDOWN, VK_F2, 0);
            if (!debugoverlay::isVisible()) throw std::runtime_error("F2 did not enable stats.");
            SendMessageW(window, WM_KEYDOWN, VK_F2, 1LL << 30);
            if (!debugoverlay::isVisible()) throw std::runtime_error("Repeated F2 toggled stats.");
            SendMessageW(window, WM_KEYDOWN, VK_F2, 0);
            if (debugoverlay::isVisible()) throw std::runtime_error("F2 did not hide stats.");
            SendMessageW(window, WM_KEYDOWN, VK_F2, 0);
        }
        std::string error;
        if (!capturePath.empty()) {
            if (captureNewGameFlow) {
                saves = defaults;
                const auto initialSave = saves.working;
                for (unsigned frame = 0; frame < 400; ++frame) advance(true);
                startPressed = true;
                advance(true);
                if (!menuActive) throw std::runtime_error("New-game test did not enter file selection.");
                for (unsigned frame = 0; frame < 120; ++frame) advance(true);
                menuInput = {0x100, 0};
                advance(true);
                if (!openingActive || openingDestination != 2 || !gameEntry.initialized || gameEntry.slot != 0)
                    throw std::runtime_error("Empty file did not initialize the gameplay opening.");
                for (unsigned frame = 0; frame < 300; ++frame) advance(true);
                draw(false);
                gamerenderer::capture(capturePath);
                for (unsigned frame = 0; frame < 2000 && openingActive; ++frame) advance(true);
                if (!gameplayActive || menuActive || saves.working != initialSave)
                    throw std::runtime_error("New-game destination or save preservation failed.");
                const auto initialUpdates = gameplayScene->updates();
                for (unsigned frame = 0; frame < 240; ++frame) advance(true);
                if (gameplayScene->updates() != initialUpdates + 120)
                    throw std::runtime_error("Gameplay simulation did not preserve its 30 Hz clock.");
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".foyer.png");
                gameplayScene.reset();
                gameplayActive = false;
                enterOpening(2, gameEntry.cutscene);
                openingSkip = true;
                for (unsigned frame = 0; frame < 33; ++frame) advance(true);
                if (!gameplayActive) throw std::runtime_error("New-game skip lost the gameplay destination.");
                for (unsigned frame = 0; frame < 240; ++frame) advance(true);
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".skip-foyer.png");
                if (saves.working != initialSave) throw std::runtime_error("Foyer reload changed the memory-card working image.");
                SendMessageW(window, WM_KEYDOWN, VK_F1, 0);
                if (gameplayScene || gameplayActive || openingActive || menuActive || gameEntry.initialized || saves.working != initialSave)
                    throw std::runtime_error("F1 did not reset the active game to fresh startup.");
                for (unsigned frame = 0; frame < 400; ++frame) advance(true);
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".reset-title.png");
                close();
                return 0;
            }
            if (captureOpeningFlow) {
                std::ofstream timings(capturePath.wstring() + L".timings.txt");
                auto entry = std::chrono::steady_clock::now();
                double maximum = 0;
                for (unsigned frame = 0; frame < 2000 && !openingActive; ++frame) advance(true);
                if (!openingActive) throw std::runtime_error("Idle title did not enter opening.");
                for (unsigned frame = 0; frame < 300; ++frame) advanceOpening();
                draw(false);
                gamerenderer::capture(capturePath);
                for (unsigned frame = 0; frame < 2000 && openingActive; ++frame) advanceOpening();
                if (openingActive || scene.isBoot() || scene.requestedScene() != -1) throw std::runtime_error("Opening did not return to title.");
                entry = std::chrono::steady_clock::now();
                enterOpening();
                timings << "Prepared opening entry ms: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entry).count() << '\n';
                for (unsigned frame = 0; frame < 1430 && openingActive; ++frame) {
                    entry = std::chrono::steady_clock::now();
                    advanceOpening();
                    if (openingActive && (profileOpening || frame % 30 == 0)) {
                        opening.draw(false);
                        gamerenderer::waitForFrame();
                        const auto completed = std::chrono::steady_clock::now();
                        opening.draw(false);
                        gamerenderer::waitForFrame();
                        timings << "Frame " << frame << " complete frame ms " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - completed).count() << '\n';
                    }
                    maximum = std::max(maximum, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entry).count());
                }
                timings << "Maximum sampled update plus two render submissions ms: " << maximum << '\n';
                openingSkip = true;
                advanceOpening();
                if (!openingActive || !openingFade.active()) throw std::runtime_error("Opening skip bypassed outgoing fade.");
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".fade-start.bmp");
                for (unsigned frame = 0; frame < 15; ++frame) advanceOpening();
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".fade-half.bmp");
                for (unsigned frame = 0; frame < 15; ++frame) advanceOpening();
                if (!openingActive || !openingFade.finished()) throw std::runtime_error("Opening fade duration changed.");
                draw(false);
                gamerenderer::capture(capturePath.wstring() + L".fade-black.bmp");
                advanceOpening();
                if (openingActive || scene.isBoot()) throw std::runtime_error("Opening skip did not return to title.");
                for (unsigned frame = 0; frame < 400; ++frame) advance(true);
                startPressed = true;
                advance(true);
                if (!menuActive) throw std::runtime_error("Menu failed after opening return.");
                draw(false);
                close();
                return 0;
            }
            if (captureDoorMaterial) {
                gameassets::Package package;
                openingscene::Resources resources;
                std::vector<std::uint8_t> executable, table;
                if (!package.open(assetPath, error) || !openingscene::read(package, 3, resources, error) ||
                    !package.read("sys/main.dol", executable, error) ||
                    !dolresources::read(executable, glme01::sineTable, 5120 * 4, table, error)) throw std::runtime_error(error);
                std::vector<float> trigonometry(5120);
                for (unsigned i = 0; i < trigonometry.size(); ++i) {
                    const unsigned offset = i * 4;
                    const std::uint32_t bits = (std::uint32_t(table[offset]) << 24) | (std::uint32_t(table[offset + 1]) << 16) |
                        (std::uint32_t(table[offset + 2]) << 8) | table[offset + 3];
                    std::memcpy(&trigonometry[i], &bits, sizeof(float));
                }
                const auto& segment = resources.segments.front();
                const auto found = std::find_if(segment.models.begin(), segment.models.end(), [](const auto& model) {
                    return model.model.name == "d01_mdl.mdl";
                });
                if (found == segment.models.end()) throw std::runtime_error("Door material capture model is missing.");
                s3dmodel::Model model;
                s3danimation::Animation animation;
                s3denvironment::Environment environment;
                s3denvironment::CameraAnimation camera;
                if (!model.read(found->model.data, error) || !animation.read(found->animations[0].at(0).data, error) ||
                    !environment.read(segment.scene.data, segment.settings.data, error) ||
                    !camera.read(segment.scene.data, segment.settings.data, error)) throw std::runtime_error(error);
                const auto textures = gamerenderer::loadPreviews({model}, 640, 480);
                const auto view = camera.sample(0);
                gamerenderer::drawPreview(0, model, model.animate(animation.sample(0), animation.flags, trigonometry),
                    environment.sample(0), &view, true, 3, false, trigonometry);
                startupscene::Picture picture;
                picture.texture = textures[0];
                picture.width = 640;
                picture.height = 480;
                gamerenderer::drawInterface({picture}, false);
                gamerenderer::capture(capturePath);
                close();
                return 0;
            }
            if (openingFrame >= 0) {
                if (!opening.load(assetPath, cutsceneId, error)) throw std::runtime_error(error);
                for (int frame = 0; frame <= openingFrame; ++frame) opening.update();
                opening.draw(false);
                gamerenderer::capture(capturePath);
                close();
                return 0;
            }
            if (captureMenuFlow) {
                std::ofstream timings(capturePath.wstring() + L".timings.txt");
                double maximum = 0;
                for (unsigned frame = 0; frame < 600 && !menuActive; ++frame) {
                    if (frame == 400) startPressed = true;
                    const auto entry = std::chrono::steady_clock::now();
                    advance(true);
                    maximum = std::max(maximum, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entry).count());
                }
                timings << "Maximum title/menu update ms: " << maximum << '\n';
                if (!menuActive) throw std::runtime_error("Title did not enter the live menu.");
                for (unsigned frame = 0; frame < 100; ++frame) { advance(true); draw(false); }
                for (unsigned i = 0; i < 3; ++i) {
                    menuInput = {2,2};
                    advance(true);
                }
                for (unsigned frame = 0; frame < 60; ++frame) advance(true);
                menuInput = {0x100,0};
                const auto entry = std::chrono::steady_clock::now();
                advance(true);
                draw(false);
                timings << "Options entry and render submission ms: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entry).count() << '\n';
                if (menuState.mode != 20) throw std::runtime_error("Live menu did not open options.");
                for (unsigned frame = 0; frame < 120; ++frame) { advance(true); draw(false); }
                gamerenderer::capture(capturePath);
                menuInput = {0x200,0};
                advance(true);
                for (unsigned frame = 0; frame < 120; ++frame) advance(true);
                menuInput = {0x200,0};
                advance(true);
                if (menuActive || scene.isBoot()) throw std::runtime_error("Menu return did not enter title directly.");
                draw(false);
                DestroyWindow(window);
                return 0;
            }
            if (fileFrame >= 0) {
                fileselect::Scene captureScene;
                fileselect::State state;
                if (!captureScene.load(assetPath, error)) throw std::runtime_error(error);
                captureScene.open();
                for (int frame = 0; frame <= fileFrame; ++frame) {
                    state.update({}, {false,false,false}, 0, false, true);
                    captureScene.update(state, saves);
                    captureScene.draw(false);
                }
                gamerenderer::capture(capturePath);
                DestroyWindow(window);
                return 0;
            }
            if (previewFrame >= 0) {
                gameassets::Package package;
                std::vector<std::uint8_t> compressed, archive, data, table;
                std::vector<s3dmodel::Model> models(4);
                std::vector<s3danimation::Animation> animations(4);
                std::vector<s3denvironment::Environment> environments(4);
                s3denvironment::MaterialAnimation materialAnimation;
                if (!package.open(assetPath, error) || !package.read("files/Kawano/ENGLISH/res_slct.szp", compressed, error) ||
                    !roomarchive::decompress(compressed, archive, error) || !package.read("sys/main.dol", data, error) ||
                    !dolresources::read(data, glme01::sineTable, 5120 * 4, table, error)) throw std::runtime_error(error);
                std::vector<float> trigonometry(5120);
                for (std::size_t i = 0; i < trigonometry.size(); ++i) {
                    const auto offset = i * 4;
                    const std::uint32_t bits = (std::uint32_t(table[offset]) << 24) | (table[offset + 1] << 16) |
                        (table[offset + 2] << 8) | table[offset + 3];
                    std::memcpy(&trigonometry[i], &bits, 4);
                }
                const char* names[]{"k_select", "select", "optdemo1", "optdemo2"};
                for (std::size_t i = 0; i < models.size(); ++i) {
                    if (!roomarchive::read(archive, std::string(names[i]) + ".mdl", data, error) || !models[i].read(data, error) ||
                        !roomarchive::read(archive, std::string(names[i]) + ".key", data, error) || !animations[i].read(data, error)) throw std::runtime_error(error);
                    if (i > 0) {
                        std::vector<std::uint8_t> settings;
                        if (!roomarchive::read(archive, std::string(names[i]) + ".scd", data, error) ||
                            !roomarchive::read(archive, std::string(names[i]) + ".sco", settings, error) ||
                            !environments[i].read(data, settings, error)) throw std::runtime_error(error);
                    }
                }
                if (!roomarchive::read(archive, "k_select.clk", data, error) ||
                    !materialAnimation.read(data, models[0].materials.size(), error)) throw std::runtime_error(error);
                materialAnimation.apply(float(previewFrame), models[0].materials);
                const auto textures = gamerenderer::loadPreviews(models);
                std::vector<startupscene::Picture> pictures;
                for (std::size_t i = 0; i < models.size(); ++i) {
                    const auto frame = std::fmod(previewFrame * (i == 1 ? 1.0f : 0.5f), float(animations[i].frames));
                    gamerenderer::drawPreview(i, models[i], models[i].animate(animations[i].sample(frame), animations[i].flags, trigonometry), environments[i].sample(frame));
                    startupscene::Picture picture;
                    picture.texture = textures[i];
                    picture.x = float(i % 2) * 296 + 24;
                    picture.y = float(i / 2) * 232 + 8;
                    picture.width = 296;
                    picture.height = 232;
                    pictures.push_back(picture);
                }
                gamerenderer::drawInterface(pictures, false);
                gamerenderer::capture(capturePath);
                DestroyWindow(window);
                return 0;
            }
            if (captureMenu >= 0) {
                gameassets::Package package;
                std::vector<std::uint8_t> compressed, archive, data;
                menulayout::Layout layout;
                menulayout::Graphics graphics;
                gamefont::Font font;
                const char* paths[]{"blo/file_select_1.blo", "blo/file_select_2.blo", "blo/option_1.blo"};
                if (!package.open(assetPath, error) || !package.read("files/Kawano/ENGLISH/res_slct.szp", compressed, error) ||
                    !roomarchive::decompress(compressed, archive, error) || !roomarchive::read(archive, paths[captureMenu], data, error) ||
                    !menulayout::read(data, layout, error) || !graphics.load(archive, layout, error) ||
                    !package.read("sys/main.dol", data, error) || !font.readExecutable(data, error)) throw std::runtime_error(error);
                graphics.setFont(font);
                if (optionsFrame >= 0) {
                    fileselect::OptionsPanes options;
                    if (!options.read(data, error)) throw std::runtime_error(error);
                    options.reset(layout);
                    if (!graphics.load(archive, layout, error, options.textures())) throw std::runtime_error(error);
                    if (!graphics.loadOptionsPortrait(assetPath, error)) throw std::runtime_error(error);
                    graphics.setFont(font);
                    fileselect::State state;
                    state.mode = 20;
                    for (int frame = 0; frame < optionsFrame; ++frame) options.update(layout, state);
                }
                gamerenderer::setInterfaceTextures(graphics.textures);
                gamerenderer::drawInterface(graphics.pictures(layout), false);
                gamerenderer::capture(capturePath);
                DestroyWindow(window);
                return 0;
            }
            if (captureAudio) {
                startupaudio::playLogo(resources.logoSamples);
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (!startupaudio::finished()) {
                    if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Boot audio did not complete.");
                    Sleep(1);
                }
            }
            const auto audioFrames = static_cast<int>(std::ceil(resources.logoSamples.size() / 2.0 / 32028.5 * 60));
            for (int frame = 0; frame < captureFrame; ++frame) advance(frame >= audioFrames);

            draw(false);
            gamerenderer::capture(capturePath);
            DestroyWindow(window);
            return 0;
        }
        throw std::runtime_error("A capture option is required.");
    } catch (const std::exception& exception) {
        close();
        if (!capturePath.empty()) {
            std::ofstream log(capturePath.wstring() + L".error.txt");
            log << exception.what();
        } else {
            MessageBoxA(nullptr, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    CaptureApplication application;
    return application.run(instance);
}
