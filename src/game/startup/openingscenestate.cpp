#include "game/startup/openingscenestate.h"

void openingscene::State::reset(unsigned destination, bool flag7) {
    *this = State{};
    returnScene = destination;
    cutscene = destination == 2 && flag7 ? 0 : 0x4c;
    stream = cutscene == 0 ? 0xc0000000 : 0xc0000001;
}

bool openingscene::State::update(bool loaded, bool finished, unsigned pressed) {
    if (requestedScene != -1) return false;
    if (phase == 1 && loaded) phase = 2;
    if (phase == 2) {
        phase = 4;
        return true;
    }
    if (phase == 4 && (finished || (pressed & 0x100) != 0)) requestedScene = static_cast<int>(returnScene);
    return false;
}
