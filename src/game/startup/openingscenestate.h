#pragma once

namespace openingscene {
    struct State {
        unsigned cutscene = 0x4c;
        unsigned stream = 0xc0000001;
        unsigned returnScene = 1;
        unsigned phase = 1;
        int requestedScene = -1;

        void reset(unsigned destination, bool flag7);
        bool update(bool loaded, bool finished, unsigned pressed);
    };
}
