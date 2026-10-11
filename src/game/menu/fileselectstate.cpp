#include "game/menu/fileselectstate.h"

#include <algorithm>

void fileselect::State::update(Input input, const std::array<bool, 3>& used, unsigned cardStatus, bool damaged, bool ready) {
    sounds.clear();
    requests.clear();
    if (requestedScene != -1) return;
    frames = std::min(frames + 1, 10U);
    if (timer > 0) --timer;
    const auto sound = [this](unsigned id) { sounds.push_back(id); };
    const auto normal = [this] { mode = 0; };
    if (mode != 20 && cardStatus != 7) {
        if (cardStatus == 1) {
            mode = 11;
        } else if (damaged) {
            mode = 21;
        } else if (mode == 6 || mode == 10 || mode == 17) {
            if (cardStatus == 0) {
                if (timer == 0) {
                    mode = mode == 6 ? 4 : mode == 10 ? 8 : 15;
                    timer = 300;
                } else if ((input.pressed & 0x1100) != 0) {
                    timer = 1;
                    sound(0x3802);
                }
            } else {
                mode = mode == 6 ? 5 : mode == 10 ? 9 : 16;
            }
        } else if (mode != 4 && mode != 5 && mode != 8 && mode != 9 && mode != 12 && mode != 14 &&
            mode != 15 && mode != 16 && mode != 18 && mode != 19) {
            if (cardStatus == 0 && mode != 0 && mode != 3 && mode != 7) normal();
            else if (cardStatus == 2 && mode != 13) { mode = 13; confirmation = 0; }
            else if (cardStatus == 3 || cardStatus == 8) mode = 12;
            else if (cardStatus == 4) mode = 18;
            else if (cardStatus == 5) mode = 19;
        }
    }
    if (mode > 10 && mode < 20) {
        if (selection < 3) selection = 4;
    } else if (selection == 4) selection = 0;
    cursorPhase += 5;
    const bool next = (input.repeat & 0x2000002) != 0;
    const bool previous = (input.repeat & 0x1000001) != 0;
    const bool up = (input.repeat & 0x8000008) != 0;
    const bool down = (input.repeat & 0x4000004) != 0;
    const bool confirm = (input.pressed & 0x1100) != 0;
    const bool cancel = (input.pressed & 0x200) != 0;
    const bool acknowledge = (input.pressed & 0x1300) != 0;
    if (mode == 3) {
        bool moved = false;
        if (next) {
            for (unsigned i = copyDestination + 1; i < 3; ++i) {
                if (!used[i]) { copyDestination = i; moved = true; }
            }
        }
        if (!moved && previous) {
            for (int i = static_cast<int>(copyDestination) - 1; i >= 0; --i) {
                if (!used[i]) { copyDestination = static_cast<unsigned>(i); moved = true; }
            }
        }
        if (moved) sound(0x3801);
    } else if (mode == 7 || mode == 13 || mode == 14) {
        if (up) { if (confirmation != 0) sound(0x3801); confirmation = 0; }
        if (down) { if (confirmation != 1) sound(0x3801); confirmation = 1; }
    } else if (mode == 20) {
        if (!rumbleOpen) {
            if (up && optionSelection > 0) { --optionSelection; sound(0x3801); }
            if (down && optionSelection < 5) { ++optionSelection; sound(0x3801); }
            if (optionSelection < 3) {
                const std::array<unsigned, 3> counts{3, 2, 2};
                auto& value = options[optionSelection];
                if (next) { value = (value + 1) % counts[optionSelection]; sound(0x3801); }
                if (previous) { value = (value + counts[optionSelection] - 1) % counts[optionSelection]; sound(0x3801); }
            }
            requests.push_back({Action::applyOptions});
        }
    } else if (mode != 4 && mode != 5 && mode != 6 && mode != 8 && mode != 9 && mode != 10 &&
        mode != 15 && mode != 16 && mode != 17 && mode != 21) {
        const bool restricted = mode == 11 || mode == 12 || mode == 18 || mode == 19;
        if (next) {
            if (restricted) {
                if (selection == 4) { sound(0x3801); cursorPhase = 255; }
                selection = 3;
            } else if (selection < 3) { ++selection; sound(0x3801); cursorPhase = 255; }
        }
        if (previous) {
            if (restricted) {
                if (selection == 3) { sound(0x3801); cursorPhase = 255; }
                selection = 4;
            } else if (selection > 0) { --selection; sound(0x3801); cursorPhase = 255; }
        }
    }
    if (mode == 0 && selection < 3 && used[selection] && (input.pressed & 0xc00) != 0) {
        if ((input.pressed & 0x800) != 0) {
            for (unsigned i = 0; i < 3; ++i) {
                if (i != selection && !used[i]) { copyDestination = i; mode = 3; sound(0x3802); break; }
            }
        } else {
            mode = 7;
            confirmation = 1;
            sound(0x3802);
        }
    } else if (mode == 0 || mode == 11 || mode == 12 || mode == 18 || mode == 19) {
        if (ready && frames == 10) {
            if (cancel) { sound(0x3803); requestedScene = 1; return; }
            if (confirm) {
                sound(0x3802);
                if (selection == 3) {
                    mode = 20;
                    optionSelection = 0;
                } else {
                    selectedFile = selection < 3 ? static_cast<int>(selection) : -1;
                    requests.push_back({Action::selectFile, selection % 4});
                    requestedScene = used[selection % 4] ? 2 : 4;
                }
            }
        }
    } else if (mode == 3) {
        if (confirm) { sound(0x3802); requests.push_back({Action::copy, selection, copyDestination}); mode = 6; timer = 100; }
        if (cancel) { sound(0x3803); normal(); }
    } else if (mode == 4 || mode == 8 || mode == 15) {
        if (acknowledge || timer == 0) { sound(0x3802); normal(); }
    } else if (mode == 5 || mode == 9 || mode == 16) {
        if (acknowledge) { sound(0x3802); mode = 12; }
    } else if (mode == 7) {
        if (confirm) {
            if (confirmation == 0) { sound(0x3802); requests.push_back({Action::erase, selection}); mode = 10; timer = 100; }
            else { sound(0x3803); normal(); }
        }
        if (cancel) { sound(0x3803); normal(); }
    } else if (mode == 13 || mode == 14) {
        const auto old = mode;
        if (confirm) {
            if (confirmation == 0) {
                sound(0x3802);
                if (old == 13) { mode = 14; confirmation = 1; }
                else { requests.push_back({Action::format}); mode = 17; timer = 100; }
            } else { sound(0x3803); mode = old == 13 ? 12 : 13; confirmation = 0; }
        } else if (cancel) { sound(0x3803); mode = old == 13 ? 12 : 13; confirmation = 0; }
    } else if (mode == 20) {
        if (cancel || confirm) {
            if (rumbleOpen) { rumbleOpen = false; requests.push_back({Action::closeRumble}); }
            else if (cancel || optionSelection == 4) { sound(cancel ? 0x3803 : 0x3802); normal(); }
            else if (optionSelection == 2) { rumbleOpen = true; sound(0x3802); requests.push_back({Action::openRumble}); }
            else if (optionSelection == 5) { sound(0x3802); requestedScene = 7; }
        }
    } else if (mode == 21 && acknowledge) {
        sound(0x3802);
        requests.push_back({Action::acknowledgeDamage});
        normal();
    }
}
