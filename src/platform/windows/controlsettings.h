#pragma once

#include <windows.h>

namespace controlsettings {
    struct ParticleDebug {
        bool hideTrailDust = false, hideBreath = false;
        bool outlineTrailDust = false, outlineBreath = false;
    };

    const ParticleDebug& particleDebug();
    void initialize(HWND window);
    bool visible();
    void setVisible(bool value);
    bool handleMessage(UINT message, WPARAM key, LPARAM flags);
    void draw();
}
