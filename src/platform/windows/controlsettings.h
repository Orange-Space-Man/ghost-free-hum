#pragma once

#include <windows.h>

namespace controlsettings {
    void initialize(HWND window);
    bool visible();
    void setVisible(bool value);
    bool handleMessage(UINT message, WPARAM key, LPARAM flags);
    void draw();
}
