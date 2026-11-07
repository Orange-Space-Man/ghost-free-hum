#include "game/scene/gameplayscene.h"
#include "game/events/entrancescript.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Entrance checks require the imported package.");
        gameassets::Package package;
        std::vector<std::uint8_t> executable, initialFlags;
        std::string error;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
            dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error), error.c_str());
        gamesave::State saves;
        gameentry::State entry;
        require(gamesave::initialize(initialFlags, saves, error) && gameentry::selectNewFile(saves, 0, entry, error), error.c_str());
        gameplay::ResourceOwner resources(argv[1], entry);
        auto& managers = resources.managers();
        auto* player = managers.playerEvents().player(0);
        auto lastCamera = resources.publishCamera().camera.position;
        s3denvironment::Camera savedCamera;
        unsigned cameraChanges = 0;
        for (unsigned frame = 0; frame < 422; ++frame) {
            managers.lights().beginFrame();
            resources.updateEvents(frame / 30, frame + 1);
            if (frame == 416) {
                const auto restored = resources.publishCamera().camera;
                require(restored.position == savedCamera.position && restored.target == savedCamera.target &&
                    restored.fieldOfView == savedCamera.fieldOfView, "CAMENDFAST must restore the retained camera before follow resumes.");
            }
            resources.updateCamera();
            const auto published = resources.publishCamera();
            require(published.visibilityMode == (frame >= 5 && frame < 416 ? 2 : 0),
                "The cinematic visibility mode must follow camera ownership and return to normal after release.");
            const auto camera = published.camera;
            if (frame == 4) savedCamera = camera;
            for (float value : camera.position) require(std::isfinite(value), "Entrance camera position must be finite.");
            if (frame >= 5 && frame < 416 && camera.position != lastCamera) ++cameraChanges;
            lastCamera = camera.position;
            require((player->state.field106C != 0) == (frame >= 3 && frame < 417),
                "Original commands must stop and release Luigi on their exact script frames.");
            if (frame >= 6 && frame <= 421) require(managers.lights().count() == 2, "Original event path light must enter the shared pool.");
            if (frame >= 4) require(managers.music().currentSound() == 0x80000019, "Entrance must select original BGM table entry ten.");
            require(resources.entranceActive() == (frame < 421), "Event ownership must survive END and its message-close stage.");
        }
        std::cout << "Entrance camera changes: " << cameraChanges << '\n';
        require(cameraChanges == 343, "Original camera keys must preserve motion and stationary holds.");
        require(entry.inventory[0x20 + 48] == 1 && !managers.flags().value(3) &&
            managers.missionEvents().activation().value(48) == 0, "Completion count must advance without setting progression flag three.");
        resources.updateEvents(15, 423);
        require(!resources.entranceActive(), "Completed entrance must not replay.");
        {
            gameplay::ResourceOwner restored(argv[1], entry);
            restored.updateEvents(0, 1);
            require(!restored.entranceActive(), "Saved completion must disable entrance activation after owner reconstruction.");
        }
        {
            gameentry::State progressed;
            require(gameentry::selectNewFile(saves, 0, progressed, error), error.c_str());
            gameflags::State flags;
            flags.restore(progressed.flags);
            flags.setOn(3);
            flags.store(progressed.flags);
            gameplay::ResourceOwner restored(argv[1], progressed);
            restored.updateEvents(0, 1);
            require(!restored.entranceActive() && progressed.inventory[0x20 + 48] == 0,
                "Progression flag three must prevent entrance without manufacturing a completion count.");
        }
        jaudio::Resources audio;
        require(jaudio::read(package, audio, error), error.c_str());
        jaudio::DspMixer mixer;
        require(mixer.configure(audio.effects, error), error.c_str());
        bool audible = false;
        for (unsigned block = 0; block < 6000; ++block) {
            std::array<std::int16_t, 160> samples;
            mixer.begin();
            require(resources.managers().playback()->mix(mixer, error), error.c_str());
            mixer.end(samples, managers.music().playback().outputLevel());
            for (auto sample : samples) audible |= sample != 0;
        }
        require(audible && managers.music().playback().notes() > 0,
            "Entrance sequence must produce PCM through the live shared music/effect mixer.");
        managers.playback()->stop();
        require(managers.music().currentSound() == 0, "Scene audio shutdown must release entrance music.");
        gameentry::State fresh;
        require(gameentry::selectNewFile(saves, 0, fresh, error), error.c_str());
        gameplay::Scene scene(argv[1], fresh);
        for (unsigned frame = 0; frame < 4; ++frame) scene.update({}, 0, frame + 1);
        const auto origin = *scene.playerView().position;
        playerinput::PadStatus held;
        held.stickX = 87;
        for (unsigned frame = 4; frame < 417; ++frame) scene.update(held, 0, frame + 1);
        require(std::abs((*scene.playerView().position)[0] - origin[0]) < 0.001f,
            "Held movement must not move Luigi while the entrance event owns control.");
        for (unsigned frame = 417; frame < 477; ++frame) scene.update(held, 0, frame + 1);
        require(std::abs((*scene.playerView().position)[0] - origin[0]) > 10,
            "Original live player scheduling must resume movement after LUIGIFREE2.");
        std::cout << "Entrance camera, light, PCM, completion, replay prevention and live control handoff passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
