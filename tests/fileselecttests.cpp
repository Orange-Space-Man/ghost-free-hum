#include "game/menu/fileselectstate.h"
#include "game/startup/openingscenestate.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

int main() {
    try {
        fileselect::State menu;
        const std::array<bool, 3> empty{};
        for (unsigned i = 0; i < 9; ++i) menu.update({0x100, 0}, empty, 0, false, true);
        check(menu.requestedScene == -1, "Menu accepted input before its ten-update gate.");
        menu.update({0x100, 0}, empty, 0, false, true);
        check(menu.requestedScene == 4 && menu.selectedFile == 0, "New game did not request the opening.");
        menu = {};
        menu.frames = 10;
        menu.update({0x1000, 0}, {true, false, false}, 0, false, true);
        check(menu.requestedScene == 2, "Used file did not request main game.");
        menu = {};
        menu.frames = 10;
        menu.update({0x200, 0}, empty, 0, false, true);
        check(menu.requestedScene == 1, "B did not return to title.");
        menu = {};
        menu.update({0x800, 0}, {false, true, false}, 0, false, true);
        check(menu.mode == 0, "Copy was available on an empty file.");
        menu.selection = 1;
        menu.update({0x800, 0}, {false, true, false}, 0, false, true);
        check(menu.mode == 3 && menu.copyDestination == 0, "Copy did not select the first empty destination.");
        menu.update({0, 2}, {false, true, false}, 0, false, true);
        check(menu.copyDestination == 2, "Copy cursor entered an occupied destination.");
        menu.mode = 3;
        menu.copyDestination = 0;
        menu.update({0, 2}, empty, 0, false, true);
        check(menu.copyDestination == 2, "Original farthest-empty destination scan changed.");
        menu.update({0x100, 0}, empty, 0, false, true);
        check(menu.mode == 6 && menu.timer == 100 && menu.requests[0].action == fileselect::Action::copy, "Copy request/wait state changed.");
        menu.update({0x100, 0}, empty, 0, false, true);
        check(menu.mode == 6 && menu.timer == 1, "Copy completion acknowledgement did not shorten its wait.");
        menu.update({}, empty, 0, false, true);
        check(menu.mode == 4 && menu.timer == 300, "Copy success did not enter its original timed message.");
        menu = {};
        menu.update({0x400, 0}, {true, false, false}, 0, false, true);
        check(menu.mode == 7 && menu.confirmation == 1, "Erase did not default to No.");
        menu.update({0x100, 0}, {true, false, false}, 0, false, true);
        check(menu.mode == 0 && menu.requests.empty(), "Default erase confirmation deleted a file.");
        menu.update({0x400, 0}, {true, false, false}, 0, false, true);
        menu.update({0x100, 8}, {true, false, false}, 0, false, true);
        check(menu.mode == 10 && menu.requests[0].action == fileselect::Action::erase, "Confirmed erase did not enter its save wait.");
        menu = {};
        menu.update({}, empty, 2, false, true);
        check(menu.mode == 13 && menu.confirmation == 0, "Broken-card prompt did not initialize.");
        menu.update({0x100, 0}, empty, 2, false, true);
        check(menu.mode == 14 && menu.confirmation == 1 && menu.requests.empty(), "Formatting did not require a second confirmation.");
        menu.update({0x100, 8}, empty, 2, false, true);
        check(menu.mode == 17 && menu.requests[0].action == fileselect::Action::format, "Second format confirmation failed.");
        menu = {};
        menu.update({}, empty, 0, true, true);
        check(menu.mode == 21, "Save-data damage did not select the original message state.");
        menu.update({0x200, 0}, empty, 0, true, true);
        check(menu.mode == 0 && menu.requests[0].action == fileselect::Action::acknowledgeDamage, "Damaged-data acknowledgement failed.");

        openingscene::State opening;
        opening.reset(1, true);
        check(opening.cutscene == 0x4c && opening.stream == 0xc0000001, "Idle demo depended on save flag 7.");
        opening.update(false, false, 0x100);
        check(opening.requestedScene == -1, "Opening skipped during loading.");
        check(opening.update(true, false, 0), "Loaded opening did not request playback start.");
        opening.update(true, false, 0x1000);
        check(opening.requestedScene == -1, "Start was substituted for the original A-only skip.");
        opening.update(true, false, 0x100);
        check(opening.requestedScene == 1, "Idle opening skip did not return to title.");
        opening.reset(2, true);
        check(opening.cutscene == 0 && opening.stream == 0xc0000000, "New-game opening flag branch changed.");
        opening.update(true, false, 0);
        opening.update(true, true, 0);
        check(opening.requestedScene == 2, "New-game opening completion did not enter main game.");
        std::cout << "Menu input, card prompts, copy/erase confirmation, and opening return-flow checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
