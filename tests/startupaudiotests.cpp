#include "platform/windows/startupaudio.h"
#include "game/startup/startupresources.h"
#include "game/audio/jaudiopositionalplayback.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void pump(unsigned milliseconds) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (std::chrono::steady_clock::now() < deadline) {
            startupaudio::update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main(int argc, char** argv) {
    jaudio::PositionOwners positionalOwners;
    try {
        check(argc == 2, "Supply a local asset package for the audio-device check.");
        startupresources::Resources resources;
        std::string error;
        check(startupresources::read(argv[1], resources, error), error.c_str());
        startupaudio::prepare(argv[1], resources.audio);
        startupaudio::playLogo(resources.logoSamples);
        pump(1300);
        check(startupaudio::finished(), "Logo audio did not complete.");
        startupaudio::playTitle();
        pump(2000);
        startupaudio::playStartSound();
        check(!startupaudio::startSoundFinished(), "Start sound did not queue.");
        pump(1500);
        check(startupaudio::startSoundFinished(), "Start sound did not finish.");
        check(startupaudio::finished() && !startupaudio::titleFinished(), "Logo and music completion were coupled.");
        startupaudio::playStartSound();
        startupaudio::playLogo(resources.logoSamples);
        check(startupaudio::startSoundFinished(), "Restart left the Start sound queued.");
        check(startupaudio::titleFinished(), "Restart left title music queued.");
        pump(1300);
        startupaudio::playTitle();
        const auto start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        while (!startupaudio::titleFinished()) {
            pump(1);
            check(std::chrono::steady_clock::now() - start < std::chrono::seconds(35), "Title playback did not finish.");
        }
        check(std::chrono::steady_clock::now() - start > std::chrono::seconds(27), "Title playback ended early.");
        startupaudio::playStartSound();
        pump(1500);
        check(startupaudio::startSoundFinished(), "Start sound failed after music completion.");
        startupaudio::playMenu();
        startupaudio::playStartSound();
        pump(100);
        startupaudio::playTitle();
        check(!startupaudio::startSoundFinished(), "Music switch interrupted the shared Start sound.");
        pump(1500);
        check(startupaudio::startSoundFinished(), "Shared Start sound did not complete after music switch.");
        startupaudio::playMenu();
        pump(2000);
        check(!startupaudio::titleFinished(), "Menu music stopped early.");
        for (const auto id : {0x3801U,0x3802U,0x3803U}) {
            startupaudio::playMenuEffect(id);
            pump(1500);
            check(startupaudio::startSoundFinished(), "Menu effect did not finish.");
        }
        startupaudio::playTitle();
        pump(250);
        check(!startupaudio::titleFinished(), "Return to title did not restart its music.");
        gameassets::Package package;
        jaudio::StreamAudio stream;
        check(package.open(argv[1], error), error.c_str());
        check(jaudio::readStream(package, "files/AudioRes/Stream/TMOpen.afc", stream, error), error.c_str());
        startupaudio::playStream(stream);
        pump(1200);
        check(startupaudio::streamFrame() >= 30 && startupaudio::streamFrame() < 45, "Opening audio clock did not advance at 30 fps.");
        startupaudio::playTitle();
        check(startupaudio::streamFrame() == 0, "Title return retained opening clock.");
        std::vector<std::uint8_t> dol;
        check(package.read("sys/main.dol", dol, error), error.c_str());
        auto positional = std::make_shared<jaudio::PositionalPlayback>(positionalOwners, dol, package, resources.audio);
        startupaudio::bindPositionalPlayback(positional);
        const std::array<float, 3> position{-100, 0, -200};
        positional->play(0x181e, position);
        for (unsigned frame = 0; frame < 45; ++frame) {
            positional->update({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
            pump(33);
        }
        check(positional->finished() && !positionalOwners.categories[1], "Positional sound did not complete through the live shared audio mixer.");
        startupaudio::bindPositionalPlayback({});
        startupaudio::close();
        startupaudio::close();
        std::cout << "Live title playback, completion, and restart checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        startupaudio::close();
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
