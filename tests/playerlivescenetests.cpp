#include "game/scene/gameplayscene.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    float distance(const std::array<float, 3>& first, const std::array<float, 3>& second) {
        const float x = first[0] - second[0], z = first[2] - second[2];
        return std::sqrt(x * x + z * z);
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Live scene checks require the imported package.");
        gameassets::Package package;
        std::vector<std::uint8_t> executable, flags;
        std::string error;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
            dolresources::read(executable, glme01::initialSaveFlags, 31, flags, error), error.c_str());
        gamesave::State saves;
        require(gamesave::initialize(flags, saves, error), error.c_str());
        for (unsigned direction = 0; direction < 4; ++direction) {
            gameentry::State entry;
            require(gameentry::selectNewFile(saves, 0, entry, error), error.c_str());
            gameplay::Scene scene(argv[1], entry);
            const auto origin = *scene.playerView().position;
            for (unsigned tick = 0; tick < 30; ++tick) scene.update({}, 0, tick);
            require(distance(origin, *scene.playerView().position) < 0.001f, "The live scene must retain the original idle spawn.");
            for (unsigned tick = 0; tick < 180; ++tick) {
                playerinput::PadStatus pad;
                if (direction < 2) pad.stickX = direction ? -87 : 87;
                else pad.stickY = direction == 2 ? 87 : -87;
                scene.update(pad, 0, tick + 30);
                for (float coordinate : *scene.playerView().position)
                    require(std::isfinite(coordinate), "Live player position must remain finite.");
            }
            const auto moved = *scene.playerView().position;
            require(distance(origin, moved) > 10, "Normal scene updates must move Luigi from actual pad input.");
            for (unsigned tick = 0; tick < 90; ++tick) scene.update({}, 0, tick + 210);
            require(scene.updates() == 300 && scene.playerView().life == 100,
                "The first-visit live schedule must advance once per tick without unexplained damage.");
            std::cout << "Live direction " << direction << ": " << distance(origin, moved) << " units, 300 ticks\n";
        }
        gameentry::State entry;
        require(gameentry::selectNewFile(saves, 0, entry, error), error.c_str());
        gameplay::Scene scene(argv[1], entry);
        for (unsigned direction = 0; direction < 8; ++direction) {
            gameentry::State interactionEntry;
            require(gameentry::selectNewFile(saves, 0, interactionEntry, error), error.c_str());
            gameplay::Scene interactionScene(argv[1], interactionEntry);
            std::cout << "Live moving interactions " << direction << std::endl;
            for (unsigned frame = 0; frame < 420; ++frame) {
                playerinput::PadStatus pad;
                if (direction < 2 || direction >= 4) pad.stickX = direction % 2 ? -87 : 87;
                if (direction >= 2) pad.stickY = direction == 3 || direction >= 6 ? -87 : 87;
                if (frame % 45 == 30) pad.buttons = 0x100;
                interactionScene.update(pad, 0, frame);
                for (float coordinate : *interactionScene.playerView().position)
                    require(std::isfinite(coordinate), "Moving interactions must preserve a finite player position.");
            }
        }
        unsigned tick = 0;
        for (unsigned button : {0x100u, 0x200u, 0x400u, 0x800u, 0x10u, 0x20u, 0x40u}) {
            std::cout << "Live button " << button << std::endl;
            for (unsigned frame = 0; frame < 90; ++frame) {
                playerinput::PadStatus pad;
                if (frame < 15) pad.buttons = static_cast<std::uint16_t>(button);
                scene.update(pad, 0, tick++);
            }
        }
        require(scene.updates() == tick, "First-visit button input must preserve the live simulation clock.");
        std::cout << "Live first-visit movement and button checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
