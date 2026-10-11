#include "game/player/playercontrolstate.h"

void playercontrols::Settings::configure(int mode) {
    if (mode < 0 || mode >= 3) return;
    fields = {
        0x100, 0, 0x200, 7, 8, 0x1000, 0x800, 0x400, 0, 0x8000005, 0x8000005, 0x8000005,
        0x8000005, 0, 0, 0x8000005, 0x8000005, 0, 0, 0x8000006, 0x8000002, 0x8000004, 4, 6,
        0, 0, 0, 0x200, 0, 5, 0x40, 0, 0, 0, 1, 2, 1, 0, 1, 0, 0, 0, 3, 0, 1, 0, 0, 0, 3, 0, 1, 0, 0, 0
    };
    fields[0x25] = fields[0x28] = mode == 0 ? 2 : 1;
    fields[0x2e] = fields[0x2b] = mode == 0 ? 1 : 2;
    flags = mode < 2 ? 0xe3 : 0xa3;
}

playercontrols::State::State(const Settings* settings) {
    reset();
    history.fill(motions[0]);
    configure(settings);
}

void playercontrols::State::reset() {
    flags0.fill(0);
    field4 = field8 = 0;
    flagsC.fill(0);
    field20 = 0;
    flags24.fill(0);
    field2C = 0;
    flags30.fill(0);
    fieldC8 = field2C;
    fieldCC = flags30[0];
    motions.fill(Motion{});
    fieldC4 = fieldC5 = 0;
}

void playercontrols::State::configure(const Settings* value) {
    if (value) settings = *value;
    else settings.configure(mode);
}

void playercontrols::State::setMode(int value) {
    mode = value;
    settings.configure(mode);
}
